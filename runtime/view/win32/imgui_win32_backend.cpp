#include "runtime/view/win32/imgui_win32_backend.h"

#include <windows.h>

#include "third_party/imgui/backends/imgui_impl_win32.h"
#include "third_party/imgui/imgui.h"

// imgui_impl_win32.h intentionally omits this declaration (behind #if 0) to
// avoid dragging <windows.h> into that header -- its own comment instructs
// callers to copy this forward declaration into a .cpp that already
// includes <windows.h>, which this one does.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace warbandlib::runtime::view::win32 {

bool ImGuiWin32Backend::Init(void* hwnd) { return ImGui_ImplWin32_Init(hwnd); }

void ImGuiWin32Backend::Shutdown() { ImGui_ImplWin32_Shutdown(); }

void ImGuiWin32Backend::NewFrame() { ImGui_ImplWin32_NewFrame(); }

bool ImGuiWin32Backend::HandleMessage(void* hwnd, std::uint32_t message, std::uintptr_t wparam,
                                       std::intptr_t lparam) {
	ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), message, static_cast<WPARAM>(wparam),
	                                static_cast<LPARAM>(lparam));

	const ImGuiIO& io = ImGui::GetIO();
	const bool is_mouse_message = message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
	const bool is_keyboard_message = message >= WM_KEYFIRST && message <= WM_KEYLAST;
	if (is_mouse_message && io.WantCaptureMouse) {
		return true;
	}
	if (is_keyboard_message && io.WantCaptureKeyboard) {
		return true;
	}
	return false;
}

} // namespace warbandlib::runtime::view::win32
