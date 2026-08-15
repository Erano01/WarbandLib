#pragma once

namespace warbandlib::runtime::view::d3d9 {

// Returns the HWND (as void*, so this header stays free of windows.h/d3d9.h)
// the device's swap chain was created against, via
// IDirect3DDevice9::GetCreationParameters. Used to resolve the window to
// install runtime/view/win32/wndproc_hook.h's WndProcHook on -- ordinary
// D3D9 API use, not manual vtable poking, since we only need to read the
// result, not intercept the call. Returns nullptr on failure.
void* ResolveFocusWindow(void* device);

} // namespace warbandlib::runtime::view::d3d9
