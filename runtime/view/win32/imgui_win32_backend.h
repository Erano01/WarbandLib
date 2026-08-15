#pragma once

#include <cstdint>

namespace warbandlib::runtime::view::win32 {

// Thin glue around Dear ImGui's Win32 backend (third_party/imgui/backends),
// mirroring runtime/view/d3d9/imgui_d3d9_backend.h's rationale: keeps ImGui
// backend headers out of runtime/view/view_layer.cpp. hwnd is void* so this
// header stays free of windows.h.
class ImGuiWin32Backend {
public:
	bool Init(void* hwnd);
	void Shutdown();
	void NewFrame();

	// Forwards a window message to ImGui's Win32 backend, then reports
	// whether ImGui wants to consume it (per
	// ImGuiIO::WantCaptureMouse/WantCaptureKeyboard for the message's
	// range). The caller (runtime/view/view_layer.cpp, via
	// runtime/view/win32/wndproc_hook.h) decides what "consume" means for
	// its own input-passthrough policy.
	bool HandleMessage(void* hwnd, std::uint32_t message, std::uintptr_t wparam, std::intptr_t lparam);
};

} // namespace warbandlib::runtime::view::win32
