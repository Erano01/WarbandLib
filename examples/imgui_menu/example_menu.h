#pragma once

namespace warbandlib::examples::imgui_menu {

// runtime::view::ViewLayer::MenuCallback-compatible entry point: one real
// ImGui window (framerate, a checkbox, a button) proving the full
// runtime/view/ path end-to-end (device, DX9/Win32 backends, input
// passthrough), rather than ImGui's own demo window.
void DrawMenu();

} // namespace warbandlib::examples::imgui_menu
