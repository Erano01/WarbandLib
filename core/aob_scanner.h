#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace warbandlib::core {

// A single byte to match, or a wildcard that matches any byte.
struct PatternByte {
	std::uint8_t value = 0;
	bool is_wildcard = false;
};

// Parses an IDA-style pattern string, e.g. "48 8B ?? 05 A4". Each token is a
// two hex digit byte, or "?"/"??" for a wildcard. Throws std::invalid_argument
// on malformed input.
std::vector<PatternByte> ParsePattern(std::string_view pattern);

// Scans [haystack, haystack + haystack_len) for the first occurrence of
// pattern. Returns the offset of the match start, or std::nullopt if not found.
std::optional<std::size_t> Find(const std::uint8_t* haystack, std::size_t haystack_len,
                                 const std::vector<PatternByte>& pattern);

// Convenience overload that parses the pattern string on every call.
std::optional<std::size_t> Find(const std::uint8_t* haystack, std::size_t haystack_len,
                                 std::string_view pattern);

} // namespace warbandlib::core
