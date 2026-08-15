#include "runtime/view/view_layer.h"

#include <windows.h>

#include <cstdint>
#include <memory>

#include "runtime/view/d3d9/device_reset_hook.h"
#include "runtime/view/d3d9/imgui_d3d9_backend.h"
#include "runtime/view/d3d9/window_query.h"
#include "runtime/view/win32/imgui_win32_backend.h"
#include "runtime/view/win32/wndproc_hook.h"
#include "third_party/imgui/imgui.h"

namespace warbandlib::runtime::view {

namespace {

core::Logger* g_logger = nullptr;
ViewLayer::MenuCallback g_menu_callback = nullptr;

bool g_initialized = false;
bool g_setup_failed = false;
bool g_visible = false;

d3d9::ImGuiD3D9Backend g_d3d9_backend;
win32::ImGuiWin32Backend g_win32_backend;
std::unique_ptr<d3d9::DeviceResetHook> g_reset_hook;
std::unique_ptr<win32::WndProcHook> g_wndproc_hook;

// F10 edge-detect (not level-detect), same idiom as
// examples/overlay_quad/quad_overlay.cpp's F9 toggle -- different key so
// both examples can be injected together during testing without clashing.
bool ConsumeToggleKeyPress() {
	static bool was_down = false;
	const bool is_down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
	const bool edge = is_down && !was_down;
	was_down = is_down;
	return edge;
}

bool HandleWindowMessage(void* hwnd, std::uint32_t message, std::uintptr_t wparam, std::intptr_t lparam) {
	if (!g_visible) {
		return false; // full passthrough while hidden
	}
	return g_win32_backend.HandleMessage(hwnd, message, wparam, lparam);
}

void OnPreReset(void* /*device*/) { g_d3d9_backend.InvalidateDeviceObjects(); }

void OnPostReset(void* /*device*/, long result) {
	if (result >= 0) { // SUCCEEDED
		g_d3d9_backend.CreateDeviceObjects();
	}
}

void EnsureInitialized(void* device, const core::win32::SignatureData& signature) {
	if (g_initialized || g_setup_failed) {
		return;
	}

	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr; // no stray imgui.ini in the game dir

	if (!g_d3d9_backend.Init(device)) {
		if (g_logger != nullptr) {
			g_logger->Write("ERROR: ImGui D3D9 backend init failed");
		}
		ImGui::DestroyContext();
		g_setup_failed = true;
		return;
	}

	void* hwnd = d3d9::ResolveFocusWindow(device);
	if (hwnd != nullptr && g_win32_backend.Init(hwnd)) {
		g_wndproc_hook = std::make_unique<win32::WndProcHook>(g_logger);
		g_wndproc_hook->Install(hwnd, &HandleWindowMessage);
	} else if (g_logger != nullptr) {
		g_logger->Write("ERROR: could not resolve focus window / init Win32 backend, no input capture");
	}

	g_reset_hook = std::make_unique<d3d9::DeviceResetHook>(g_logger, &OnPreReset, &OnPostReset);
	g_reset_hook->Install(device, signature.reset_vtbl_index);

	g_initialized = true;
	if (g_logger != nullptr) {
		g_logger->Write("ViewLayer initialized");
	}
}

} // namespace

void ViewLayer::Configure(core::Logger* logger, MenuCallback menu_callback) {
	g_logger = logger;
	g_menu_callback = menu_callback;
}

void ViewLayer::Tick(void* device, const core::win32::SignatureData& signature) {
	EnsureInitialized(device, signature);
	if (!g_initialized) {
		return;
	}

	if (ConsumeToggleKeyPress()) {
		g_visible = !g_visible;
		if (g_logger != nullptr) {
			g_logger->Write(g_visible ? "F10: view layer shown" : "F10: view layer hidden");
		}
	}

	if (!g_visible) {
		return;
	}

	g_d3d9_backend.NewFrame();
	g_win32_backend.NewFrame();

	// ImGui_ImplWin32_NewFrame() derives DisplaySize from the window's
	// client rect, which can transiently report a smaller-than-real size
	// during an Alt-Tab minimize/restore or device-reset cycle. ImGui
	// clamps window positions to fit whatever DisplaySize it's given each
	// frame (ClampWindowPos in imgui.cpp), so a single bad frame here would
	// yank a user-moved window toward the corner and leave it there.
	// Override with the real D3D9 backbuffer size, which is stable through
	// that transition.
	float backbuffer_width = 0.0f;
	float backbuffer_height = 0.0f;
	if (g_d3d9_backend.QueryBackbufferSize(device, &backbuffer_width, &backbuffer_height) &&
	    backbuffer_width > 0.0f && backbuffer_height > 0.0f) {
		ImGui::GetIO().DisplaySize = ImVec2(backbuffer_width, backbuffer_height);
	}

	ImGui::NewFrame();

	if (g_menu_callback != nullptr) {
		g_menu_callback();
	}

	ImGui::Render();
	g_d3d9_backend.Render();
}

void ViewLayer::Shutdown() {
	if (!g_initialized) {
		return;
	}

	g_reset_hook.reset();
	if (g_wndproc_hook != nullptr) {
		g_wndproc_hook.reset();
		g_win32_backend.Shutdown();
	}
	g_d3d9_backend.Shutdown();
	if (ImGui::GetCurrentContext() != nullptr) {
		ImGui::DestroyContext();
	}

	g_initialized = false;
	g_visible = false;
	if (g_logger != nullptr) {
		g_logger->Write("ViewLayer shut down");
	}
}

} // namespace warbandlib::runtime::view
