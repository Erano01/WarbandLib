#include "runtime/graphics/d3d9/endscene_hook.h"

#include <windows.h>

#include <cstdint>

#include "core/win32/memory.h"
#include "runtime/hooking/vtable_hook.h"

namespace warbandlib::runtime::graphics::d3d9 {

namespace {

// COM/D3D9 ABI: __stdcall, callee cleans the stack, "this" passed as the
// first explicit parameter. We don't link against d3d9.h/d3d9.lib -- only
// the calling convention and vtable slot index matter here.
using EndSceneFn = long(__stdcall*)(void* device);

// Single active hook instance for this process. Milestone 0 only ever
// installs one EndScene hook per injected DLL.
EndSceneFn g_original_end_scene = nullptr;
core::Logger* g_logger = nullptr;
EndSceneHook::TickCallback g_tick_callback = nullptr;
core::win32::SignatureData g_signature;

long __stdcall EndSceneDetour(void* device) {
	static std::uint64_t tick = 0;
	static std::uint64_t last_logged_tick = 0;
	++tick;

	// Throttle to roughly once a second at ~60 fps instead of logging every
	// frame.
	if (tick - last_logged_tick >= 60) {
		last_logged_tick = tick;
		if (g_logger != nullptr) {
			g_logger->Write("EndScene tick (hook alive)");
		}
	}

	if (g_tick_callback != nullptr) {
		g_tick_callback(device, g_signature);
	}

	return g_original_end_scene(device);
}

} // namespace

EndSceneHook::EndSceneHook(core::Logger* logger, TickCallback tick_callback)
    : logger_(logger), tick_callback_(tick_callback) {
	g_logger = logger_;
	g_tick_callback = tick_callback_;
}

EndSceneHook::~EndSceneHook() { Uninstall(); }

bool EndSceneHook::TryInstall(const core::win32::Module& module,
                               const core::win32::SignatureData& signature) {
	if (hook_ != nullptr) {
		return true; // already installed
	}

	void* instance_va = module.Va(signature.app_instance);
	void* device_ptr_address =
	    reinterpret_cast<std::uint8_t*>(instance_va) + signature.app_device_offset;
	void* device = core::win32::ReadPtr(device_ptr_address);

	if (device == nullptr) {
		return false; // device not created yet -- caller should retry
	}

	hook_ = std::make_unique<hooking::VTableHook>(device, signature.endscene_vtbl_index,
	                                               reinterpret_cast<void*>(&EndSceneDetour));
	if (!hook_->Install()) {
		hook_.reset();
		return false;
	}

	g_original_end_scene = reinterpret_cast<EndSceneFn>(hook_->original());
	if (logger_ != nullptr) {
		logger_->Write("EndScene hook installed");
	}
	return true;
}

void EndSceneHook::Uninstall() {
	if (hook_ != nullptr) {
		hook_->Uninstall();
		hook_.reset();
		if (logger_ != nullptr) {
			logger_->Write("EndScene hook uninstalled");
		}
	}
}

} // namespace warbandlib::runtime::graphics::d3d9
