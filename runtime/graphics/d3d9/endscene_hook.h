#pragma once

#include <memory>

#include "core/logging.h"
#include "core/win32/module.h"

namespace warbandlib::runtime::hooking {
class VTableHook;
}

namespace warbandlib::runtime::graphics::d3d9 {

// Installs a vtable hook on IDirect3DDevice9::EndScene, resolved via the
// device pointer chain documented in re/mb_warband.exe.md and
// signatures/mb_warband.ini (g_pMyD3DApplication + app_device_offset).
//
// An optional per-frame callback can be supplied so other code (e.g. an
// examples/ overlay) can reuse this same resolve+hook mechanism instead of
// duplicating it. The callback receives the raw IDirect3DDevice9* (as
// void*, so this header stays free of d3d9.h) and runs on every frame,
// right before the original EndScene is called through.
class EndSceneHook {
public:
	using TickCallback = void (*)(void* device);

	explicit EndSceneHook(core::Logger* logger, TickCallback tick_callback = nullptr);
	~EndSceneHook();

	EndSceneHook(const EndSceneHook&) = delete;
	EndSceneHook& operator=(const EndSceneHook&) = delete;

	// Reads the device pointer via module/signature and installs the hook.
	// Returns false if the device pointer is still null (caller should
	// retry) or the vtable write failed.
	bool TryInstall(const core::win32::Module& module, const core::win32::SignatureData& signature);

	void Uninstall();

private:
	core::Logger* logger_;
	TickCallback tick_callback_;
	std::unique_ptr<warbandlib::runtime::hooking::VTableHook> hook_;
};

} // namespace warbandlib::runtime::graphics::d3d9
