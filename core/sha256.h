#pragma once

#include <cstdint>
#include <string>

namespace warbandlib::core {

// Minimal, dependency-free SHA-256. Returns the digest as a lowercase hex
// string (64 chars). Used to fingerprint the target exe against the
// signatures/ offset table before trusting any hardcoded offset.
std::string Sha256Hex(const std::uint8_t* data, std::size_t len);

} // namespace warbandlib::core
