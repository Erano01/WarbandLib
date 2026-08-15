#pragma once

#include <cstdint>

namespace warbandlib::core::win32 {

// RAII guard that flips a memory range to PAGE_EXECUTE_READWRITE for the
// guard's lifetime, then restores the original protection on destruction.
class ScopedProtect {
public:
	ScopedProtect(void* address, std::size_t size);
	~ScopedProtect();

	ScopedProtect(const ScopedProtect&) = delete;
	ScopedProtect& operator=(const ScopedProtect&) = delete;

	// True if VirtualProtect succeeded on construction.
	bool ok() const { return ok_; }

private:
	void* address_;
	std::size_t size_;
	unsigned long old_protect_ = 0;
	bool ok_ = false;
};

// Reads a pointer-sized value at address. Caller is responsible for ensuring
// the address is valid to read (e.g. resolved via core::win32::Module).
void* ReadPtr(const void* address);

// Writes a pointer-sized value at address under a ScopedProtect. Returns
// false if the protection change failed.
bool WritePtr(void* address, void* value);

} // namespace warbandlib::core::win32
