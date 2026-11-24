#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "runtime/host/semver.h"

namespace warbandlib::host {

bool IsValidPluginId(std::string_view id);

struct PluginDependency {
	std::string id;
	std::optional<Version> min_version;
	bool optional = false;
};

struct PluginNode {
	std::string id;
	Version version;
	std::vector<PluginDependency> dependencies;
};

struct PluginRejection {
	std::size_t index;
	std::string reason;
};

struct LoadPlan {
	std::vector<std::size_t> order; // indices into the input, dependencies first
	std::vector<PluginRejection> rejected;
};

// Input order breaks ties, so a sorted directory listing gives a stable plan.
LoadPlan PlanLoadOrder(const std::vector<PluginNode>& nodes);

} // namespace warbandlib::host
