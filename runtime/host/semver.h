#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace warbandlib::host {

struct Version {
	std::uint32_t major = 0;
	std::uint32_t minor = 0;
	std::uint32_t patch = 0;
};

// "1", "1.2", "1.2.3"; a "-pre" or "+build" suffix is accepted and ignored.
std::optional<Version> ParseVersion(std::string_view text);

int CompareVersions(const Version& a, const Version& b);

} // namespace warbandlib::host
