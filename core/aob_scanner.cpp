#include "core/aob_scanner.h"

#include <cctype>
#include <stdexcept>

namespace warbandlib::core {

namespace {

int HexNibble(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	throw std::invalid_argument("aob pattern: invalid hex digit");
}

} // namespace

std::vector<PatternByte> ParsePattern(std::string_view pattern) {
	std::vector<PatternByte> result;

	std::size_t i = 0;
	while (i < pattern.size()) {
		if (std::isspace(static_cast<unsigned char>(pattern[i]))) {
			++i;
			continue;
		}

		if (pattern[i] == '?') {
			++i;
			if (i < pattern.size() && pattern[i] == '?') {
				++i;
			}
			result.push_back(PatternByte{0, true});
			continue;
		}

		if (i + 1 >= pattern.size()) {
			throw std::invalid_argument("aob pattern: dangling hex digit");
		}

		const int hi = HexNibble(pattern[i]);
		const int lo = HexNibble(pattern[i + 1]);
		result.push_back(PatternByte{static_cast<std::uint8_t>((hi << 4) | lo), false});
		i += 2;
	}

	if (result.empty()) {
		throw std::invalid_argument("aob pattern: empty pattern");
	}

	return result;
}

std::optional<std::size_t> Find(const std::uint8_t* haystack, std::size_t haystack_len,
                                 const std::vector<PatternByte>& pattern) {
	if (pattern.empty() || haystack_len < pattern.size()) {
		return std::nullopt;
	}

	const std::size_t last_start = haystack_len - pattern.size();
	for (std::size_t start = 0; start <= last_start; ++start) {
		bool matched = true;
		for (std::size_t j = 0; j < pattern.size(); ++j) {
			if (!pattern[j].is_wildcard && haystack[start + j] != pattern[j].value) {
				matched = false;
				break;
			}
		}
		if (matched) {
			return start;
		}
	}

	return std::nullopt;
}

std::optional<std::size_t> Find(const std::uint8_t* haystack, std::size_t haystack_len,
                                 std::string_view pattern) {
	return Find(haystack, haystack_len, ParsePattern(pattern));
}

} // namespace warbandlib::core
