#pragma once

#include <windows.h>

#include "warbandlib/warbandlib.h"

namespace warbandlib::runtime::overlay {

// Hooks Present/Reset through a throwaway device's vtable, which d3d9.dll shares with the game's device.
bool Install(HMODULE self);
// Puts back every vtable slot / window procedure that still points to us.
void Uninstall();
// Releases ImGui and D3D objects; call once the render thread has left the detours.
void Shutdown();

// Posts a message that makes the game's window thread run queued game-thread tasks.
bool WakeGameThread();
DWORD GameWindowThread();

const wbl_ui_api* UiApi();
const wbl_draw_api* DrawApi();

} // namespace warbandlib::runtime::overlay
