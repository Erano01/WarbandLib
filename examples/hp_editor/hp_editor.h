#pragma once

namespace warbandlib::examples::hp_editor {

// runtime::view::ViewLayer::MenuCallback-compatible entry point: scans
// process memory for live Agent instances (see re/mb_warband.exe.md) and
// lets the user read/adjust Agent::hit_points on whichever one they pick --
// a live test bed for the offset found via Cheat Engine.
void DrawMenu();

} // namespace warbandlib::examples::hp_editor
