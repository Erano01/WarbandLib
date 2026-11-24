#include "runtime/host/semver.h"

namespace warbandlib::host {

std::optional<Version> ParseVersion(std::string_view text) {
	const std::size_t suffix = text.find_first_of("-+");
	if (suffix != std::string_view::npos) {
		text = text.substr(0, suffix);
	}
	if (text.empty()) {
		return std::nullopt;
	}

	std::uint32_t parts[3] = {0, 0, 0};
	int part = 0;
	bool have_digit = false;
	for (const char c : text) {
		if (c == '.') {
			if (!have_digit || ++part > 2) {
				return std::nullopt;
			}
			have_digit = false;
			continue;
		}
		if (c < '0' || c > '9') {
			return std::nullopt;
		}
		const std::uint64_t next = static_cast<std::uint64_t>(parts[part]) * 10u + static_cast<std::uint32_t>(c - '0');
		if (next > 0xFFFFFFFFu) {
			return std::nullopt;
		}
		parts[part] = static_cast<std::uint32_t>(next);
		have_digit = true;
	}
	if (!have_digit) {
		return std::nullopt;
	}
	return Version{parts[0], parts[1], parts[2]};
}

int CompareVersions(const Version& a, const Version& b) {
	if (a.major != b.major) return a.major < b.major ? -1 : 1;
	if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
	if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
	return 0;
}

} // namespace warbandlib::host
