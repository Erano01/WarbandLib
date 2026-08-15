#include "core/win32/memory.h"

#include <windows.h>

#include <cstring>

namespace warbandlib::core::win32 {

ScopedProtect::ScopedProtect(void* address, std::size_t size) : address_(address), size_(size) {
	DWORD old_protect = 0;
	ok_ = VirtualProtect(address_, size_, PAGE_EXECUTE_READWRITE, &old_protect) != 0;
	old_protect_ = old_protect;
}

ScopedProtect::~ScopedProtect() {
	if (ok_) {
		DWORD unused = 0;
		VirtualProtect(address_, size_, old_protect_, &unused);
	}
}

void* ReadPtr(const void* address) {
	void* value = nullptr;
	std::memcpy(&value, address, sizeof(value));
	return value;
}

bool WritePtr(void* address, void* value) {
	ScopedProtect guard(address, sizeof(void*));
	if (!guard.ok()) {
		return false;
	}
	std::memcpy(address, &value, sizeof(value));
	return true;
}

} // namespace warbandlib::core::win32
