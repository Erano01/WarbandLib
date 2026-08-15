#pragma once

#include <cstddef>

namespace warbandlib::runtime::hooking {

// Swaps a single slot in a COM-style vtable (an array of function pointers
// pointed to by the first 4 bytes of any interface instance) for the
// lifetime of this object. Install()/Uninstall() are idempotent.
class VTableHook {
public:
	// interface_ptr must point to a valid COM-style interface instance
	// (its first pointer-sized field is the vtable pointer).
	VTableHook(void* interface_ptr, std::size_t vtable_index, void* detour);
	~VTableHook();

	VTableHook(const VTableHook&) = delete;
	VTableHook& operator=(const VTableHook&) = delete;

	bool Install();
	void Uninstall();

	// The original function pointer that was in the slot before Install().
	// Only valid after a successful Install().
	void* original() const { return original_; }

private:
	void** vtable_ = nullptr;
	std::size_t index_;
	void* detour_;
	void* original_ = nullptr;
	bool installed_ = false;
};

} // namespace warbandlib::runtime::hooking
