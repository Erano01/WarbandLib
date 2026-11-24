// Raw Direct3D 9 drawing from WBL_EVENT_FRAME, toggled with F9.
#include <windows.h>
#include <d3d9.h>

#include "warbandlib/warbandlib.h"

namespace {

struct Vertex {
	float x, y, z, rhw;
	D3DCOLOR color;
};

bool g_visible = true;
bool g_was_down = false;

void Quad(IDirect3DDevice9* d, float x0, float y0, float x1, float y1, D3DCOLOR c) {
	const Vertex v[4] = {{x0, y0, 0, 1, c}, {x1, y0, 0, 1, c}, {x0, y1, 0, 1, c}, {x1, y1, 0, 1, c}};
	d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex));
}

wbl_event_reply WBL_CALL OnFrame(const wbl_event* event, void*) {
	const bool down = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
	if (down && !g_was_down) g_visible = !g_visible;
	g_was_down = down;
	if (!g_visible) return WBL_EVENT_CONTINUE;

	const auto* frame = static_cast<const wbl_frame_event*>(event->data);
	auto* device = static_cast<IDirect3DDevice9*>(frame->d3d9_device);
	IDirect3DStateBlock9* state = nullptr;
	if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &state))) return WBL_EVENT_CONTINUE;

	D3DVIEWPORT9 vp{0, 0, frame->width, frame->height, 0.0f, 1.0f};
	device->SetViewport(&vp);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	device->SetPixelShader(nullptr);
	device->SetVertexShader(nullptr);
	device->SetTexture(0, nullptr);
	device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	const float x = static_cast<float>(frame->width) - 70.0f;
	Quad(device, x, 10, x + 60, 40, D3DCOLOR_ARGB(255, 0, 0, 0));
	Quad(device, x + 4, 14, x + 56, 36, D3DCOLOR_ARGB(255, 170, 20, 20));

	state->Apply();
	state->Release();
	return WBL_EVENT_CONTINUE;
}

} // namespace

WBL_PLUGIN_QUERY {
	static const wbl_plugin_info info = {
	    sizeof(wbl_plugin_info), WBL_ABI_VERSION_MAJOR, 0, "example.overlay_quad", "Overlay quad", "1.0.0", "WarbandLib",
	    "Draws a quad with raw Direct3D 9 every frame (F9 toggles).", 0, nullptr,
	};
	return &info;
}

WBL_PLUGIN_LOAD(host, self) {
	return host->events->subscribe(self, WBL_EVENT_FRAME, WBL_PRIORITY_NORMAL, OnFrame, nullptr, nullptr);
}

WBL_PLUGIN_UNLOAD {}
