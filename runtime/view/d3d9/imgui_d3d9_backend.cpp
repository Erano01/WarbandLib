#include "runtime/view/d3d9/imgui_d3d9_backend.h"

#include <windows.h>
#include <d3d9.h>

#include "third_party/imgui/backends/imgui_impl_dx9.h"
#include "third_party/imgui/imgui.h"

namespace warbandlib::runtime::view::d3d9 {

bool ImGuiD3D9Backend::Init(void* device) {
	return ImGui_ImplDX9_Init(static_cast<IDirect3DDevice9*>(device));
}

void ImGuiD3D9Backend::Shutdown() { ImGui_ImplDX9_Shutdown(); }

void ImGuiD3D9Backend::NewFrame() { ImGui_ImplDX9_NewFrame(); }

void ImGuiD3D9Backend::Render() { ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData()); }

void ImGuiD3D9Backend::InvalidateDeviceObjects() { ImGui_ImplDX9_InvalidateDeviceObjects(); }

void ImGuiD3D9Backend::CreateDeviceObjects() { ImGui_ImplDX9_CreateDeviceObjects(); }

} // namespace warbandlib::runtime::view::d3d9
