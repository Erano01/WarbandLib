#include "runtime/host/plugin_graph.h"

#include <map>

namespace warbandlib::host {

bool IsValidPluginId(std::string_view id) {
	if (id.empty() || id.size() > 64) {
		return false;
	}
	for (const char c : id) {
		const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
		if (!ok) {
			return false;
		}
	}
	return true;
}

namespace {

std::string VersionText(const Version& v) {
	return std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);
}

} // namespace

LoadPlan PlanLoadOrder(const std::vector<PluginNode>& nodes) {
	LoadPlan plan;
	const std::size_t n = nodes.size();
	std::vector<bool> rejected(n, false);
	std::map<std::string, std::size_t> by_id;

	auto reject = [&](std::size_t i, std::string reason) {
		rejected[i] = true;
		plan.rejected.push_back({i, std::move(reason)});
	};

	for (std::size_t i = 0; i < n; ++i) {
		if (!IsValidPluginId(nodes[i].id)) {
			reject(i, "invalid plugin id '" + nodes[i].id + "'");
			continue;
		}
		const auto [it, inserted] = by_id.emplace(nodes[i].id, i);
		if (!inserted) {
			reject(i, "duplicate plugin id '" + nodes[i].id + "'");
		}
	}

	bool changed = true;
	while (changed) {
		changed = false;
		for (std::size_t i = 0; i < n; ++i) {
			if (rejected[i]) continue;
			for (const PluginDependency& dep : nodes[i].dependencies) {
				const auto it = by_id.find(dep.id);
				const bool present = it != by_id.end() && !rejected[it->second];
				if (!present) {
					if (dep.optional) continue;
					reject(i, it == by_id.end() ? "missing dependency '" + dep.id + "'"
					                            : "dependency '" + dep.id + "' was rejected");
					changed = true;
					break;
				}
				if (it->second == i) {
					reject(i, "plugin depends on itself");
					changed = true;
					break;
				}
				if (dep.min_version && CompareVersions(nodes[it->second].version, *dep.min_version) < 0) {
					const std::string reason = "needs '" + dep.id + "' >= " + VersionText(*dep.min_version) +
					                           ", found " + VersionText(nodes[it->second].version);
					if (dep.optional) {
						continue;
					}
					reject(i, reason);
					changed = true;
					break;
				}
			}
		}
	}

	// Kahn's algorithm, always taking the lowest ready index.
	std::vector<std::vector<std::size_t>> dependents(n);
	std::vector<std::size_t> pending(n, 0);
	for (std::size_t i = 0; i < n; ++i) {
		if (rejected[i]) continue;
		for (const PluginDependency& dep : nodes[i].dependencies) {
			const auto it = by_id.find(dep.id);
			if (it == by_id.end() || rejected[it->second]) continue;
			if (dep.min_version && CompareVersions(nodes[it->second].version, *dep.min_version) < 0) continue;
			dependents[it->second].push_back(i);
			++pending[i];
		}
	}

	std::vector<bool> placed(n, false);
	for (;;) {
		std::size_t next = n;
		for (std::size_t i = 0; i < n; ++i) {
			if (!rejected[i] && !placed[i] && pending[i] == 0) {
				next = i;
				break;
			}
		}
		if (next == n) break;
		placed[next] = true;
		plan.order.push_back(next);
		for (const std::size_t d : dependents[next]) {
			--pending[d];
		}
	}

	for (std::size_t i = 0; i < n; ++i) {
		if (!rejected[i] && !placed[i]) {
			plan.rejected.push_back({i, "dependency cycle"});
		}
	}
	return plan;
}

} // namespace warbandlib::host
