#pragma once

#include <cstdint>

#include "core/logging.h"

namespace warbandlib::runtime::view::win32 {

// Subclasses a window's WndProc for the lifetime of this object (RAII, same
// shape as runtime/hooking/vtable_hook.h) so callers -- here, ImGui's Win32
// backend -- can observe/consume input messages before the game's own
// WndProc runs. hwnd is void* so this header stays free of windows.h.
class WndProcHook {
public:
	// Returns true if the message was consumed (the game's own WndProc is
	// skipped for that message); false to pass it through unmodified.
	using MessageCallback = bool (*)(void* hwnd, std::uint32_t message, std::uintptr_t wparam,
	                                  std::intptr_t lparam);

	explicit WndProcHook(core::Logger* logger);
	~WndProcHook();

	WndProcHook(const WndProcHook&) = delete;
	WndProcHook& operator=(const WndProcHook&) = delete;

	bool Install(void* hwnd, MessageCallback callback);
	void Uninstall();

private:
	core::Logger* logger_;
	void* hwnd_ = nullptr;
	bool installed_ = false;
};

} // namespace warbandlib::runtime::view::win32
