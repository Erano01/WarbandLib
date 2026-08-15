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
class EndSceneHook {
public:
	explicit EndSceneHook(core::Logger* logger);
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
	std::unique_ptr<warbandlib::runtime::hooking::VTableHook> hook_;
};

} // namespace warbandlib::runtime::graphics::d3d9
