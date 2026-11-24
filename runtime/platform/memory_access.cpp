#include "runtime/platform/memory_access.h"

#include <windows.h>

#include <cstring>

namespace warbandlib::runtime {

namespace {

// Touching a guard page (another thread's stack growth marker) clears its guard bit; put it back so that
// thread can still grow its stack instead of overflowing later.
int CopyFilter(EXCEPTION_POINTERS* info) {
	const EXCEPTION_RECORD* record = info->ExceptionRecord;
	if (record->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION && record->NumberParameters >= 2) {
		void* page = reinterpret_cast<void*>(record->ExceptionInformation[1]);
		MEMORY_BASIC_INFORMATION mbi{};
		DWORD old = 0;
		if (VirtualQuery(page, &mbi, sizeof(mbi)) == sizeof(mbi)) {
			VirtualProtect(page, 1, mbi.Protect | PAGE_GUARD, &old);
		}
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

bool GuardedCopy(void* dst, const void* src, std::uint32_t size) {
	__try {
		std::memcpy(dst, src, size);
		return true;
	} __except (CopyFilter(GetExceptionInformation())) {
		return false;
	}
}

bool IsWritableProtection(DWORD protect) {
	return (protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

} // namespace

wbl_result SafeRead(std::uintptr_t address, void* out, std::uint32_t size) {
	if (out == nullptr || address == 0) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	return GuardedCopy(out, reinterpret_cast<const void*>(address), size) ? WBL_OK : WBL_ERR_ACCESS_VIOLATION;
}

wbl_result SafeWrite(std::uintptr_t address, const void* data, std::uint32_t size) {
	if (data == nullptr || address == 0) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	void* dst = reinterpret_cast<void*>(address);
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(dst, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT) {
		return WBL_ERR_ACCESS_VIOLATION;
	}
	const bool executable = (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
	                                        PAGE_EXECUTE_WRITECOPY)) != 0;
	DWORD old = 0;
	const bool reprotect = !IsWritableProtection(mbi.Protect);
	if (reprotect && !VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &old)) {
		return WBL_ERR_ACCESS_VIOLATION;
	}
	const bool ok = GuardedCopy(dst, data, size);
	if (reprotect) {
		DWORD unused = 0;
		VirtualProtect(dst, size, old, &unused);
	}
	if (executable) {
		FlushInstructionCache(GetCurrentProcess(), dst, size);
	}
	return ok ? WBL_OK : WBL_ERR_ACCESS_VIOLATION;
}

bool IsReadable(std::uintptr_t address, std::uint32_t size) {
	if (address == 0) {
		return false;
	}
	const std::uintptr_t end = address + size;
	if (end < address) {
		return false;
	}
	std::uintptr_t cursor = address;
	do {
		MEMORY_BASIC_INFORMATION mbi{};
		if (VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi)) != sizeof(mbi)) {
			return false;
		}
		if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 || mbi.Protect == 0) {
			return false;
		}
		cursor = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
	} while (cursor < end);
	return true;
}

} // namespace warbandlib::runtime
