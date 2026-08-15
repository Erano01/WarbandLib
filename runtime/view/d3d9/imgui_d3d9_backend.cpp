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

bool ImGuiD3D9Backend::QueryBackbufferSize(void* device_ptr, float* out_width, float* out_height) {
	auto* device = static_cast<IDirect3DDevice9*>(device_ptr);

	IDirect3DSurface9* render_target = nullptr;
	if (FAILED(device->GetRenderTarget(0, &render_target)) || render_target == nullptr) {
		return false;
	}

	D3DSURFACE_DESC desc{};
	const bool ok = SUCCEEDED(render_target->GetDesc(&desc));
	render_target->Release();
	if (!ok) {
		return false;
	}

	*out_width = static_cast<float>(desc.Width);
	*out_height = static_cast<float>(desc.Height);
	return true;
}

} // namespace warbandlib::runtime::view::d3d9
