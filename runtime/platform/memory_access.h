#pragma once

#include <cstdint>

#include "warbandlib/warbandlib.h"

namespace warbandlib::runtime {

wbl_result SafeRead(std::uintptr_t address, void* out, std::uint32_t size);
wbl_result SafeWrite(std::uintptr_t address, const void* data, std::uint32_t size);
bool IsReadable(std::uintptr_t address, std::uint32_t size);

} // namespace warbandlib::runtime
