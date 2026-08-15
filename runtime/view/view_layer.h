#pragma once

#include "core/logging.h"
#include "core/win32/module.h"

namespace warbandlib::runtime::view {

// Orchestrates WarbandLib's own custom-drawn UI layer (an ImGui overlay) on
// top of the existing EndScene hook: owns ImGui context setup, the DX9 and
// Win32 backends (runtime/view/d3d9, runtime/view/win32), WndProc input
// capture, and Reset (device-lost) handling. Exposes a single per-frame
// entry point that matches
// runtime::graphics::d3d9::EndSceneHook::TickCallback, so it installs the
// same way examples/overlay_quad's Tick() does. At most one instance per
// process (process-global state), same assumption EndSceneHook itself
// makes.
//
// Deliberately scoped to *our own* drawn UI, not the game's own native
// menus -- reading/driving the game's own widget system is a separate,
// harder problem (its own RE) left for the game-internals phase.
class ViewLayer {
public:
	// Draws the caller's menu content. Called every frame while the view
	// layer is visible, between ImGui::NewFrame() and ImGui::Render().
	using MenuCallback = void (*)();

	// Registers logger and menu_callback. Must be called before
	// constructing the EndSceneHook that will drive Tick() -- state is kept
	// in process-global storage, so setting it up before the hook exists
	// avoids any ordering race with the game's own render thread calling
	// Tick() the instant the hook installs.
	static void Configure(core::Logger* logger, MenuCallback menu_callback);

	// EndSceneHook::TickCallback-compatible entry point. Lazily performs
	// one-time setup (ImGui context, DX9/Win32 backends, WndProc hook,
	// Reset hook) on its first call, then runs the ImGui frame while
	// visible (F10-toggle).
	static void Tick(void* device, const core::win32::SignatureData& signature);

	// Uninstalls the WndProc/Reset hooks and tears down the ImGui
	// context/backends. Call from DLL_PROCESS_DETACH, after resetting the
	// EndSceneHook that drives Tick() so no further Tick() call can race
	// this teardown. No-op if Tick() never got far enough to initialize.
	static void Shutdown();
};

} // namespace warbandlib::runtime::view
