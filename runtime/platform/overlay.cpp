#include "runtime/platform/overlay.h"
#include "runtime/platform/game_events.h"

#include <d3d9.h>

#include <cstdint>
#include <string>

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "runtime/platform/runtime_state.h"

extern IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace warbandlib::runtime::overlay {

namespace {

using PresentFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

constexpr std::size_t kResetIndex = 16;
constexpr std::size_t kPresentIndex = 17;
constexpr int kToggleKey = VK_INSERT;

void** g_vtable = nullptr;
PresentFn g_original_present = nullptr;
ResetFn g_original_reset = nullptr;

IDirect3DDevice9* g_device = nullptr;
IDirect3DSurface9* g_back_buffer = nullptr;
HWND g_hwnd = nullptr;
WNDPROC g_original_wndproc = nullptr;
bool g_imgui_ready = false;
bool g_fonts_ready = false;
bool g_menu_visible = false;
bool g_toggle_was_down = false;

DWORD g_render_thread = 0;
bool g_in_draw = false;
bool g_in_ui = false;
LARGE_INTEGER g_qpc_frequency{};
LARGE_INTEGER g_last_frame{};
std::uint64_t g_frame_index = 0;

bool UiAllowed() { return g_in_ui && GetCurrentThreadId() == g_render_thread; }
bool DrawAllowed() { return (g_in_draw || g_in_ui) && GetCurrentThreadId() == g_render_thread; }

bool WriteSlot(std::size_t index, void* value) {
	DWORD old = 0;
	if (!VirtualProtect(&g_vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
		return false;
	}
	g_vtable[index] = value;
	VirtualProtect(&g_vtable[index], sizeof(void*), old, &old);
	return true;
}

void CacheBackBuffer(IDirect3DDevice9* device) {
	if (g_back_buffer != nullptr) {
		g_back_buffer->Release();
		g_back_buffer = nullptr;
	}
	device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_back_buffer);
}

UINT TaskMessage() {
	static const UINT id = RegisterWindowMessageA("WarbandLib.GameThreadTasks");
	return id;
}

LRESULT CALLBACK WndProcDetour(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (msg == TaskMessage() && msg != 0) {
		game_events::RunGameThreadTasks(); // the window thread is the game thread: between frames here
		return 0;
	}
	if (g_menu_visible && g_imgui_ready) {
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
		const ImGuiIO& io = ImGui::GetIO();
		const bool mouse = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSEWHEEL;
		const bool keyboard = msg >= WM_KEYFIRST && msg <= WM_KEYLAST;
		if ((mouse && io.WantCaptureMouse) || (keyboard && io.WantCaptureKeyboard)) {
			return TRUE;
		}
	}
	return CallWindowProcA(g_original_wndproc, hwnd, msg, wparam, lparam);
}

void EnsureImGui(IDirect3DDevice9* device) {
	if (g_imgui_ready) {
		return;
	}
	g_device = device;
	D3DDEVICE_CREATION_PARAMETERS cp{};
	g_hwnd = SUCCEEDED(device->GetCreationParameters(&cp)) && cp.hFocusWindow ? cp.hFocusWindow : GetForegroundWindow();

	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::StyleColorsDark();
	ImGui_ImplDX9_Init(g_hwnd, device);
	CacheBackBuffer(device);
	g_original_wndproc = reinterpret_cast<WNDPROC>(
	    SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProcDetour)));
	g_render_thread = GetCurrentThreadId();
	QueryPerformanceFrequency(&g_qpc_frequency);
	QueryPerformanceCounter(&g_last_frame);
	g_imgui_ready = true;
	Logf(WBL_LOG_INFO, "overlay", "ImGui ready, hwnd=0x%08X, toggle with INSERT",
	     static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(g_hwnd)));
	WakeGameThread(); // tasks queued before the window was known
}

bool DeviceReady() {
	if (g_device->TestCooperativeLevel() != D3D_OK) {
		return false;
	}
	if (!g_fonts_ready) {
		// ImGui 1.60 builds its font texture lazily and dereferences it even when that failed.
		g_fonts_ready = ImGui_ImplDX9_CreateDeviceObjects();
		Logf(WBL_LOG_INFO, "overlay", "ImGui device objects: %s", g_fonts_ready ? "ok" : "FAILED");
	}
	return g_fonts_ready;
}

void DrawHostWindow() {
	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("WarbandLib");
	const RuntimeState& s = State();
	ImGui::Text("runtime %s  |  ABI %u.%u", kRuntimeVersion, WBL_ABI_VERSION_MAJOR, WBL_ABI_VERSION_MINOR);
	ImGui::Text("game: %s", s.game_info.fingerprint_known ? "known build" : "unknown build");
	ImGui::Text("hooks: %u", static_cast<unsigned>(s.hooks.Count()));
	ImGui::Separator();
	for (const PluginSummary& p : s.plugins.Summaries()) {
		const bool ok = p.status == "loaded";
		const ImVec4 color = ok ? ImVec4(0.55f, 0.85f, 0.55f, 1.0f) : ImVec4(0.95f, 0.45f, 0.4f, 1.0f);
		const std::string head = (p.id.empty() ? p.file_name : p.id) + "  " + p.version;
		ImGui::TextColored(color, "%s", head.c_str());
		if (!p.name.empty()) {
			ImGui::SameLine();
			ImGui::TextDisabled("%s", p.name.c_str());
		}
		if (!ok) {
			ImGui::TextWrapped("  %s: %s", p.status.c_str(), p.error.c_str());
		}
	}
	ImGui::End();
}

void RunOverlayFrame(IDirect3DDevice9* device) {
	if (!DeviceReady()) {
		return;
	}
	const bool down = (GetAsyncKeyState(kToggleKey) & 0x8000) != 0;
	if (down && !g_toggle_was_down) {
		g_menu_visible = !g_menu_visible;
	}
	g_toggle_was_down = down;

	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	const float dt = static_cast<float>(now.QuadPart - g_last_frame.QuadPart) / static_cast<float>(g_qpc_frequency.QuadPart);
	g_last_frame = now;

	D3DSURFACE_DESC desc{};
	if (g_back_buffer != nullptr) {
		g_back_buffer->GetDesc(&desc);
	}
	wbl_frame_event frame{};
	frame.struct_size = sizeof(frame);
	frame.d3d9_device = device;
	frame.width = desc.Width;
	frame.height = desc.Height;
	frame.delta_seconds = dt;
	frame.frame_index = g_frame_index++;

	State().events.Post(WBL_EVENT_FRAME, &frame);

	ImGui_ImplDX9_NewFrame();
	ImGui::GetIO().MouseDrawCursor = g_menu_visible;
	g_in_draw = true;
	State().events.Post(WBL_EVENT_DRAW, &frame);
	g_in_draw = false;
	if (g_menu_visible) {
		DrawHostWindow();
		g_in_ui = true;
		State().events.Post(WBL_EVENT_UI, &frame);
		g_in_ui = false;
	}
	ImGui::EndFrame();
	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	if (draw_data != nullptr && draw_data->TotalVtxCount > 0) {
		ImGui_ImplDX9_RenderDrawData(draw_data);
	}
}

HRESULT __stdcall PresentDetour(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND wnd,
                                const RGNDATA* dirty) {
	EnsureImGui(device);
	if (device == g_device && g_back_buffer != nullptr) {
		IDirect3DSurface9* previous = nullptr;
		device->GetRenderTarget(0, &previous);
		if (previous != g_back_buffer) {
			device->SetRenderTarget(0, g_back_buffer);
		}
		if (SUCCEEDED(device->BeginScene())) {
			RunOverlayFrame(device);
			device->EndScene();
		}
		if (previous != nullptr) {
			if (previous != g_back_buffer) {
				device->SetRenderTarget(0, previous);
			}
			previous->Release();
		}
	}
	return g_original_present(device, src, dst, wnd, dirty);
}

HRESULT __stdcall ResetDetour(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
	wbl_device_event ev{};
	ev.struct_size = sizeof(ev);
	ev.d3d9_device = device;
	ev.present_parameters = params;
	State().events.Post(WBL_EVENT_DEVICE_RESET_BEGIN, &ev);

	// Reset fails while anyone still holds the back buffer.
	if (g_imgui_ready) {
		ImGui_ImplDX9_InvalidateDeviceObjects();
	}
	if (g_back_buffer != nullptr) {
		g_back_buffer->Release();
		g_back_buffer = nullptr;
	}
	const HRESULT hr = g_original_reset(device, params);
	Logf(WBL_LOG_INFO, "overlay", "Reset hr=0x%08lX %ux%u", static_cast<unsigned long>(hr),
	     params != nullptr ? params->BackBufferWidth : 0u, params != nullptr ? params->BackBufferHeight : 0u);
	if (g_imgui_ready && SUCCEEDED(hr)) {
		ImGui_ImplDX9_CreateDeviceObjects();
		CacheBackBuffer(device);
	}

	ev.hresult = hr;
	State().events.Post(WBL_EVENT_DEVICE_RESET_END, &ev);
	return hr;
}

ImU32 ToImColor(wbl_color c) {
	const ImU32 a = (c >> 24) & 0xFF, r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
	return IM_COL32(r, g, b, a);
}

ImVec4 ToImVec4(wbl_color c) {
	return ImVec4(static_cast<float>((c >> 16) & 0xFF) / 255.0f, static_cast<float>((c >> 8) & 0xFF) / 255.0f,
	              static_cast<float>(c & 0xFF) / 255.0f, static_cast<float>((c >> 24) & 0xFF) / 255.0f);
}

const char* Safe(const char* s) { return s != nullptr ? s : ""; }

int g_window_depth = 0;

wbl_bool WBL_CALL UiBeginWindow(const char* title, wbl_bool* open) {
	if (!UiAllowed()) return WBL_FALSE;
	bool is_open = open == nullptr || *open != 0;
	++g_window_depth;
	const bool visible = ImGui::Begin(Safe(title), open != nullptr ? &is_open : nullptr);
	if (open != nullptr) *open = is_open ? WBL_TRUE : WBL_FALSE;
	return visible ? WBL_TRUE : WBL_FALSE;
}
void WBL_CALL UiEndWindow() {
	if (!UiAllowed() || g_window_depth <= 0) return;
	--g_window_depth;
	ImGui::End();
}
void WBL_CALL UiText(const char* text) {
	if (UiAllowed()) ImGui::TextUnformatted(Safe(text));
}
void WBL_CALL UiTextColored(wbl_color color, const char* text) {
	if (UiAllowed()) ImGui::TextColored(ToImVec4(color), "%s", Safe(text));
}
wbl_bool WBL_CALL UiButton(const char* label) { return UiAllowed() && ImGui::Button(Safe(label)) ? WBL_TRUE : WBL_FALSE; }
wbl_bool WBL_CALL UiCheckbox(const char* label, wbl_bool* value) {
	if (!UiAllowed() || value == nullptr) return WBL_FALSE;
	bool v = *value != 0;
	const bool changed = ImGui::Checkbox(Safe(label), &v);
	*value = v ? WBL_TRUE : WBL_FALSE;
	return changed ? WBL_TRUE : WBL_FALSE;
}
wbl_bool WBL_CALL UiSliderInt(const char* label, std::int32_t* value, std::int32_t min, std::int32_t max) {
	if (!UiAllowed() || value == nullptr) return WBL_FALSE;
	int v = *value;
	const bool changed = ImGui::SliderInt(Safe(label), &v, min, max);
	*value = v;
	return changed ? WBL_TRUE : WBL_FALSE;
}
wbl_bool WBL_CALL UiSliderFloat(const char* label, float* value, float min, float max) {
	if (!UiAllowed() || value == nullptr) return WBL_FALSE;
	return ImGui::SliderFloat(Safe(label), value, min, max) ? WBL_TRUE : WBL_FALSE;
}
wbl_bool WBL_CALL UiSelectable(const char* label, wbl_bool selected) {
	return UiAllowed() && ImGui::Selectable(Safe(label), selected != 0) ? WBL_TRUE : WBL_FALSE;
}
wbl_bool WBL_CALL UiCollapsingHeader(const char* label) {
	return UiAllowed() && ImGui::CollapsingHeader(Safe(label)) ? WBL_TRUE : WBL_FALSE;
}
void WBL_CALL UiSameLine() {
	if (UiAllowed()) ImGui::SameLine();
}
void WBL_CALL UiSeparator() {
	if (UiAllowed()) ImGui::Separator();
}
void WBL_CALL UiPushId(const char* id) {
	if (UiAllowed()) ImGui::PushID(Safe(id));
}
void WBL_CALL UiPopId() {
	if (UiAllowed()) ImGui::PopID();
}

void WBL_CALL DrawLine(float x1, float y1, float x2, float y2, wbl_color color, float thickness) {
	if (DrawAllowed()) ImGui::GetOverlayDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), ToImColor(color), thickness);
}
void WBL_CALL DrawRect(float x1, float y1, float x2, float y2, wbl_color color, float thickness) {
	if (DrawAllowed())
		ImGui::GetOverlayDrawList()->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), ToImColor(color), 0.0f, ImDrawCornerFlags_All,
		                                     thickness);
}
void WBL_CALL DrawRectFilled(float x1, float y1, float x2, float y2, wbl_color color) {
	if (DrawAllowed()) ImGui::GetOverlayDrawList()->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), ToImColor(color));
}
void WBL_CALL DrawCircle(float x, float y, float radius, wbl_color color, float thickness) {
	if (DrawAllowed()) ImGui::GetOverlayDrawList()->AddCircle(ImVec2(x, y), radius, ToImColor(color), 32, thickness);
}
void WBL_CALL DrawTextAt(float x, float y, wbl_color color, const char* text) {
	if (DrawAllowed()) ImGui::GetOverlayDrawList()->AddText(ImVec2(x, y), ToImColor(color), Safe(text));
}

const wbl_ui_api kUiApi = {
    sizeof(wbl_ui_api), UiBeginWindow, UiEndWindow,  UiText,             UiTextColored, UiButton,    UiCheckbox, UiSliderInt,
    UiSliderFloat,      UiSelectable,  UiCollapsingHeader, UiSameLine, UiSeparator,   UiPushId,    UiPopId,
};

const wbl_draw_api kDrawApi = {sizeof(wbl_draw_api), DrawLine, DrawRect, DrawRectFilled, DrawCircle, DrawTextAt};

} // namespace

bool WakeGameThread() {
	const HWND hwnd = g_hwnd;
	return hwnd != nullptr && g_original_wndproc != nullptr && PostMessageA(hwnd, TaskMessage(), 0, 0) != FALSE;
}

DWORD GameWindowThread() {
	const HWND hwnd = g_hwnd;
	return hwnd != nullptr ? GetWindowThreadProcessId(hwnd, nullptr) : 0;
}

bool Install(HMODULE self) {
	WNDCLASSEXA wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = self;
	wc.lpszClassName = "WarbandLibDummyWindow";
	RegisterClassExA(&wc);
	HWND dummy = CreateWindowExA(0, wc.lpszClassName, "wbl", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr,
	                             self, nullptr);
	IDirect3D9* d3d = dummy != nullptr ? Direct3DCreate9(D3D_SDK_VERSION) : nullptr;
	IDirect3DDevice9* device = nullptr;
	if (d3d != nullptr) {
		D3DPRESENT_PARAMETERS pp{};
		pp.Windowed = TRUE;
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.BackBufferFormat = D3DFMT_UNKNOWN;
		d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, dummy, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
	}
	bool ok = false;
	if (device != nullptr) {
		g_vtable = *reinterpret_cast<void***>(device);
		g_original_reset = reinterpret_cast<ResetFn>(g_vtable[kResetIndex]);
		g_original_present = reinterpret_cast<PresentFn>(g_vtable[kPresentIndex]);
		ok = WriteSlot(kResetIndex, reinterpret_cast<void*>(&ResetDetour)) &&
		     WriteSlot(kPresentIndex, reinterpret_cast<void*>(&PresentDetour));
		device->Release();
	}
	if (d3d != nullptr) d3d->Release();
	if (dummy != nullptr) DestroyWindow(dummy);
	UnregisterClassA(wc.lpszClassName, self);
	Log(ok ? WBL_LOG_INFO : WBL_LOG_ERROR, "overlay", ok ? "Present/Reset hooked" : "D3D9 hook setup failed");
	return ok;
}

void Uninstall() {
	if (g_vtable != nullptr) {
		if (g_vtable[kPresentIndex] == reinterpret_cast<void*>(&PresentDetour)) {
			WriteSlot(kPresentIndex, reinterpret_cast<void*>(g_original_present));
		} else {
			Log(WBL_LOG_WARN, "overlay", "Present slot no longer ours, left as is");
		}
		if (g_vtable[kResetIndex] == reinterpret_cast<void*>(&ResetDetour)) {
			WriteSlot(kResetIndex, reinterpret_cast<void*>(g_original_reset));
		} else {
			Log(WBL_LOG_WARN, "overlay", "Reset slot no longer ours, left as is");
		}
		g_vtable = nullptr;
	}
	if (g_original_wndproc != nullptr && g_hwnd != nullptr &&
	    GetWindowLongPtrA(g_hwnd, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&WndProcDetour)) {
		SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
		g_original_wndproc = nullptr;
	}
}

void Shutdown() {
	if (g_imgui_ready) {
		ImGui_ImplDX9_Shutdown();
		ImGui::DestroyContext();
		g_imgui_ready = false;
	}
	if (g_back_buffer != nullptr) {
		g_back_buffer->Release();
		g_back_buffer = nullptr;
	}
	g_device = nullptr;
}

const wbl_ui_api* UiApi() { return &kUiApi; }
const wbl_draw_api* DrawApi() { return &kDrawApi; }

} // namespace warbandlib::runtime::overlay
