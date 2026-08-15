#pragma once

#include <cstddef>
#include <memory>

#include "core/logging.h"

namespace warbandlib::runtime::hooking {
class VTableHook;
}

namespace warbandlib::runtime::view::d3d9 {

// Installs a vtable hook on IDirect3DDevice9::Reset so callers can release
// (before) and recreate (after) device-dependent resources -- e.g. ImGui's
// DX9 backend objects -- around a real device reset (alt-tab out of
// exclusive fullscreen, resolution change), instead of touching stale
// pointers. Reuses runtime::hooking::VTableHook, same mechanism
// runtime/graphics/d3d9/endscene_hook.h uses for a different vtable slot --
// no new hook engine needed.
class DeviceResetHook {
public:
	// pre_reset runs immediately before the game's own Reset() call.
	// post_reset runs immediately after, receiving the HRESULT Reset()
	// returned (as long, to stay ABI-agnostic like
	// runtime/graphics/d3d9/endscene_hook.h does).
	using PreResetCallback = void (*)(void* device);
	using PostResetCallback = void (*)(void* device, long result);

	DeviceResetHook(core::Logger* logger, PreResetCallback pre_reset, PostResetCallback post_reset);
	~DeviceResetHook();

	DeviceResetHook(const DeviceResetHook&) = delete;
	DeviceResetHook& operator=(const DeviceResetHook&) = delete;

	// device must already be a valid, resolved IDirect3DDevice9* -- no
	// polling here, unlike EndSceneHook::TryInstall. By the time this is
	// called (from view_layer.cpp's first Tick()) the device is already
	// known-good via the EndScene hook.
	bool Install(void* device, std::size_t reset_vtbl_index);
	void Uninstall();

private:
	core::Logger* logger_;
	PreResetCallback pre_reset_;
	PostResetCallback post_reset_;
	std::unique_ptr<warbandlib::runtime::hooking::VTableHook> hook_;
};

} // namespace warbandlib::runtime::view::d3d9
