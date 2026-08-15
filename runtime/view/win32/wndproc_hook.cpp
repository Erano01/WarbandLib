#include "runtime/view/win32/wndproc_hook.h"

#include <windows.h>

namespace warbandlib::runtime::view::win32 {

namespace {

WNDPROC g_original_wndproc = nullptr;
WndProcHook::MessageCallback g_callback = nullptr;

LRESULT CALLBACK Detour(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
	if (g_callback != nullptr &&
	    g_callback(hwnd, message, static_cast<std::uintptr_t>(wparam), static_cast<std::intptr_t>(lparam))) {
		return TRUE;
	}
	return CallWindowProcA(g_original_wndproc, hwnd, message, wparam, lparam);
}

} // namespace

WndProcHook::WndProcHook(core::Logger* logger) : logger_(logger) {}

WndProcHook::~WndProcHook() { Uninstall(); }

bool WndProcHook::Install(void* hwnd, MessageCallback callback) {
	if (installed_) {
		return true;
	}

	g_callback = callback;
	SetLastError(0);
	const LONG_PTR original = SetWindowLongPtrA(static_cast<HWND>(hwnd), GWLP_WNDPROC,
	                                             reinterpret_cast<LONG_PTR>(&Detour));
	if (original == 0 && GetLastError() != 0) {
		g_callback = nullptr;
		return false;
	}

	g_original_wndproc = reinterpret_cast<WNDPROC>(original);
	hwnd_ = hwnd;
	installed_ = true;
	if (logger_ != nullptr) {
		logger_->Write("WndProc hook installed");
	}
	return true;
}

void WndProcHook::Uninstall() {
	if (!installed_) {
		return;
	}
	SetWindowLongPtrA(static_cast<HWND>(hwnd_), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original_wndproc));
	g_callback = nullptr;
	installed_ = false;
	if (logger_ != nullptr) {
		logger_->Write("WndProc hook uninstalled");
	}
}

} // namespace warbandlib::runtime::view::win32
