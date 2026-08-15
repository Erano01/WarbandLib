#include "runtime/view/d3d9/device_reset_hook.h"

#include <windows.h>

#include "runtime/hooking/vtable_hook.h"

namespace warbandlib::runtime::view::d3d9 {

namespace {

// COM/D3D9 ABI: __stdcall, "this" as the first explicit parameter. We only
// need to call through, not inspect the D3DPRESENT_PARAMETERS* argument, so
// keep it opaque like EndSceneFn does.
using ResetFn = long(__stdcall*)(void* device, void* presentation_parameters);

ResetFn g_original_reset = nullptr;
core::Logger* g_logger = nullptr;
DeviceResetHook::PreResetCallback g_pre_reset = nullptr;
DeviceResetHook::PostResetCallback g_post_reset = nullptr;

long __stdcall ResetDetour(void* device, void* presentation_parameters) {
	if (g_pre_reset != nullptr) {
		g_pre_reset(device);
	}

	const long result = g_original_reset(device, presentation_parameters);

	if (g_logger != nullptr) {
		g_logger->Write("Device Reset() intercepted");
	}
	if (g_post_reset != nullptr) {
		g_post_reset(device, result);
	}
	return result;
}

} // namespace

DeviceResetHook::DeviceResetHook(core::Logger* logger, PreResetCallback pre_reset,
                                  PostResetCallback post_reset)
    : logger_(logger), pre_reset_(pre_reset), post_reset_(post_reset) {
	g_logger = logger_;
	g_pre_reset = pre_reset_;
	g_post_reset = post_reset_;
}

DeviceResetHook::~DeviceResetHook() { Uninstall(); }

bool DeviceResetHook::Install(void* device, std::size_t reset_vtbl_index) {
	if (hook_ != nullptr) {
		return true; // already installed
	}

	hook_ = std::make_unique<hooking::VTableHook>(device, reset_vtbl_index,
	                                               reinterpret_cast<void*>(&ResetDetour));
	if (!hook_->Install()) {
		hook_.reset();
		return false;
	}

	g_original_reset = reinterpret_cast<ResetFn>(hook_->original());
	if (logger_ != nullptr) {
		logger_->Write("Device Reset hook installed");
	}
	return true;
}

void DeviceResetHook::Uninstall() {
	if (hook_ != nullptr) {
		hook_->Uninstall();
		hook_.reset();
		if (logger_ != nullptr) {
			logger_->Write("Device Reset hook uninstalled");
		}
	}
}

} // namespace warbandlib::runtime::view::d3d9
