#pragma once

namespace warbandlib::runtime::view::d3d9 {

// Thin glue around Dear ImGui's DX9 backend (third_party/imgui/backends).
// device is void* so nothing outside this .cpp needs d3d9.h, mirroring
// runtime/graphics/d3d9/endscene_hook.h's rationale. At most one instance
// per process -- ImGui's backend keeps its own state as process-globals
// internally, same assumption runtime/graphics/d3d9/endscene_hook.h makes
// about EndSceneHook.
class ImGuiD3D9Backend {
public:
	bool Init(void* device);
	void Shutdown();

	void NewFrame();
	void Render(); // Call after ImGui::Render().

	// Call immediately before/after a real IDirect3DDevice9::Reset() so
	// ImGui's DX9-resource-backed objects (fonts, vertex/index buffers)
	// survive it instead of using stale pointers. See
	// runtime/view/d3d9/device_reset_hook.h.
	void InvalidateDeviceObjects();
	void CreateDeviceObjects();
};

} // namespace warbandlib::runtime::view::d3d9
