#include "examples/overlay_quad/quad_overlay.h"

#include <windows.h>
#include <d3d9.h>

#include <cstdint>
#include <cstdio>

namespace warbandlib::examples::overlay_quad {

namespace {

struct Vertex {
	float x, y, z, rhw;
	D3DCOLOR color;
};
constexpr DWORD kVertexFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

bool g_visible = true;
core::Logger* g_logger = nullptr;

// F9 edge-detect (not level-detect) so holding the key doesn't rapid-toggle.
bool ConsumeToggleKeyPress() {
	static bool was_down = false;
	const bool is_down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
	const bool edge = is_down && !was_down;
	was_down = is_down;
	return edge;
}

void LogDiagnosticsPeriodically() {
	if (g_logger == nullptr) {
		return;
	}
	static std::uint64_t tick = 0;
	++tick;
	if (tick % 180 != 1) { // roughly every ~3s at 60fps
		return;
	}
	char buf[128];
	std::snprintf(buf, sizeof(buf), "diag: visible=%d GetAsyncKeyState(F9)=0x%04x",
	              static_cast<int>(g_visible), static_cast<unsigned>(GetAsyncKeyState(VK_F9)) & 0xFFFF);
	g_logger->Write(buf);
}

// Resets the viewport to the full backbuffer so pre-transformed screen-space
// vertices aren't clipped by whatever sub-region viewport the game left
// active (e.g. a HUD element's own viewport) when EndScene fired.
void SetFullBackbufferViewport(IDirect3DDevice9* device) {
	IDirect3DSurface9* render_target = nullptr;
	if (FAILED(device->GetRenderTarget(0, &render_target)) || render_target == nullptr) {
		return;
	}

	D3DSURFACE_DESC desc{};
	if (SUCCEEDED(render_target->GetDesc(&desc))) {
		D3DVIEWPORT9 viewport{0, 0, desc.Width, desc.Height, 0.0f, 1.0f};
		device->SetViewport(&viewport);
	}
	render_target->Release();
}

void DrawQuad(IDirect3DDevice9* device, float x0, float y0, float x1, float y1, D3DCOLOR color) {
	const Vertex quad[4] = {
	    {x0, y0, 0.f, 1.f, color},
	    {x1, y0, 0.f, 1.f, color},
	    {x0, y1, 0.f, 1.f, color},
	    {x1, y1, 0.f, 1.f, color},
	};
	device->SetFVF(kVertexFvf);
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
}

void DrawIndicatorQuad(IDirect3DDevice9* device) {
	// Big, fully opaque, high-contrast: a black border behind a dark red
	// fill, easy to spot against any in-game background.
	constexpr float kX0 = 10.f, kY0 = 10.f, kX1 = 260.f, kY1 = 80.f;
	constexpr float kBorder = 6.f;
	const D3DCOLOR border_color = D3DCOLOR_ARGB(255, 0, 0, 0);
	const D3DCOLOR fill_color = D3DCOLOR_ARGB(255, 170, 20, 20);

	SetFullBackbufferViewport(device);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetTexture(0, nullptr);

	DrawQuad(device, kX0, kY0, kX1, kY1, border_color);
	DrawQuad(device, kX0 + kBorder, kY0 + kBorder, kX1 - kBorder, kY1 - kBorder, fill_color);
}

} // namespace

void SetLogger(core::Logger* logger) { g_logger = logger; }

void Tick(void* device_ptr) {
	if (ConsumeToggleKeyPress()) {
		g_visible = !g_visible;
		if (g_logger != nullptr) {
			g_logger->Write(g_visible ? "F9: overlay shown" : "F9: overlay hidden");
		}
	}
	LogDiagnosticsPeriodically();

	if (!g_visible) {
		return;
	}

	auto* device = static_cast<IDirect3DDevice9*>(device_ptr);

	IDirect3DStateBlock9* state_block = nullptr;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state_block))) {
		return;
	}

	DrawIndicatorQuad(device);

	state_block->Apply();
	state_block->Release();
}

} // namespace warbandlib::examples::overlay_quad
