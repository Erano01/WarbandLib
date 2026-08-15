#include "runtime/hooking/vtable_hook.h"

#include "core/win32/memory.h"

namespace warbandlib::runtime::hooking {

VTableHook::VTableHook(void* interface_ptr, std::size_t vtable_index, void* detour)
    : index_(vtable_index), detour_(detour) {
	vtable_ = *reinterpret_cast<void***>(interface_ptr);
}

VTableHook::~VTableHook() { Uninstall(); }

bool VTableHook::Install() {
	if (installed_ || vtable_ == nullptr) {
		return false;
	}

	original_ = vtable_[index_];
	if (!core::win32::WritePtr(&vtable_[index_], detour_)) {
		original_ = nullptr;
		return false;
	}

	installed_ = true;
	return true;
}

void VTableHook::Uninstall() {
	if (!installed_) {
		return;
	}
	core::win32::WritePtr(&vtable_[index_], original_);
	installed_ = false;
}

} // namespace warbandlib::runtime::hooking
