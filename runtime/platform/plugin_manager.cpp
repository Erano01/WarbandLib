#include "runtime/platform/plugin_manager.h"

#include <algorithm>
#include <cstring>

#include "runtime/host/plugin_graph.h"
#include "runtime/host/semver.h"
#include "runtime/platform/runtime_state.h"

namespace warbandlib::runtime {

namespace {

constexpr std::uint32_t kMinInfoSize = static_cast<std::uint32_t>(sizeof(wbl_plugin_info));

bool GuardedQuery(wbl_plugin_query_fn fn, const wbl_plugin_info** out) {
	__try {
		*out = fn();
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool GuardedLoad(wbl_plugin_load_fn fn, const wbl_host_api* host, wbl_plugin self, wbl_result* out) {
	__try {
		*out = fn(host, self);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

bool GuardedUnload(wbl_plugin_unload_fn fn) {
	__try {
		fn();
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

std::string Str(const char* s) { return s != nullptr ? std::string(s) : std::string(); }

const char* StatusText(wbl_plugin_s::Status s) {
	switch (s) {
		case wbl_plugin_s::Status::Discovered: return "discovered";
		case wbl_plugin_s::Status::Loaded: return "loaded";
		case wbl_plugin_s::Status::Failed: return "failed";
		case wbl_plugin_s::Status::Unloaded: return "unloaded";
	}
	return "?";
}

} // namespace

void PluginManager::Fail(wbl_plugin_s& plugin, const std::string& reason) {
	plugin.status = wbl_plugin_s::Status::Failed;
	plugin.error = reason;
	Logf(WBL_LOG_ERROR, "plugins", "%s: %s", plugin.file_name.c_str(), reason.c_str());
	if (plugin.module != nullptr) {
		ReleaseOwner(&plugin);
		FreeLibrary(plugin.module);
		plugin.module = nullptr;
	}
}

bool PluginManager::Discover(const std::string& path, const std::string& file_name) {
	auto plugin = std::make_unique<wbl_plugin_s>();
	plugin->file_name = file_name;
	plugin->file_path = path;
	wbl_plugin_s& p = *plugin;
	plugins_.push_back(std::move(plugin));

	p.module = LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (p.module == nullptr) {
		Fail(p, "LoadLibrary failed, error " + std::to_string(GetLastError()));
		return false;
	}
	auto query = reinterpret_cast<wbl_plugin_query_fn>(
	    reinterpret_cast<void*>(GetProcAddress(p.module, WBL_PLUGIN_QUERY_SYMBOL)));
	p.load = reinterpret_cast<wbl_plugin_load_fn>(reinterpret_cast<void*>(GetProcAddress(p.module, WBL_PLUGIN_LOAD_SYMBOL)));
	p.unload = reinterpret_cast<wbl_plugin_unload_fn>(
	    reinterpret_cast<void*>(GetProcAddress(p.module, WBL_PLUGIN_UNLOAD_SYMBOL)));
	if (query == nullptr || p.load == nullptr) {
		Fail(p, "not a WarbandLib plugin (missing wbl_plugin_query / wbl_plugin_load)");
		return false;
	}

	const wbl_plugin_info* info = nullptr;
	if (!GuardedQuery(query, &info)) {
		Fail(p, "wbl_plugin_query crashed");
		return false;
	}
	if (info == nullptr || info->struct_size < kMinInfoSize) {
		Fail(p, "wbl_plugin_query returned no or a truncated wbl_plugin_info");
		return false;
	}
	if (info->abi_major != WBL_ABI_VERSION_MAJOR) {
		Fail(p, "built for ABI " + std::to_string(info->abi_major) + ".x, host is " +
		            std::to_string(WBL_ABI_VERSION_MAJOR) + "." + std::to_string(WBL_ABI_VERSION_MINOR));
		return false;
	}
	if (info->abi_minor > WBL_ABI_VERSION_MINOR) {
		Fail(p, "needs ABI " + std::to_string(info->abi_major) + "." + std::to_string(info->abi_minor) +
		            ", host is " + std::to_string(WBL_ABI_VERSION_MAJOR) + "." + std::to_string(WBL_ABI_VERSION_MINOR));
		return false;
	}

	p.id = Str(info->id);
	p.name = Str(info->name);
	p.version = Str(info->version);
	p.author = Str(info->author);
	p.description = Str(info->description);
	if (info->dependency_count > 0 && info->dependencies == nullptr) {
		Fail(p, "dependency_count set without dependencies");
		return false;
	}
	for (std::uint32_t i = 0; i < info->dependency_count; ++i) {
		const wbl_dependency& d = info->dependencies[i];
		if (d.struct_size < sizeof(wbl_dependency) || d.id == nullptr) {
			Fail(p, "malformed dependency #" + std::to_string(i));
			return false;
		}
		p.dependency_ids.push_back(d.id);
		p.dependency_versions.push_back(Str(d.min_version));
	}
	for (std::size_t i = 0; i < p.dependency_ids.size(); ++i) {
		wbl_dependency d{};
		d.struct_size = sizeof(d);
		d.id = p.dependency_ids[i].c_str();
		d.min_version = p.dependency_versions[i].empty() ? nullptr : p.dependency_versions[i].c_str();
		d.flags = info->dependencies[i].flags;
		p.dependencies.push_back(d);
	}
	p.info = *info;
	p.info.struct_size = sizeof(wbl_plugin_info);
	p.info.id = p.id.c_str();
	p.info.name = p.name.c_str();
	p.info.version = p.version.c_str();
	p.info.author = p.author.c_str();
	p.info.description = p.description.c_str();
	p.info.dependency_count = static_cast<std::uint32_t>(p.dependencies.size());
	p.info.dependencies = p.dependencies.empty() ? nullptr : p.dependencies.data();
	return true;
}

void PluginManager::LoadAll(const std::string& directory, const wbl_host_api* host) {
	std::vector<std::string> files;
	WIN32_FIND_DATAA fd{};
	const HANDLE find = FindFirstFileA((directory + "*.dll").c_str(), &fd);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) files.emplace_back(fd.cFileName);
		} while (FindNextFileA(find, &fd));
		FindClose(find);
	}
	std::sort(files.begin(), files.end());
	Logf(WBL_LOG_INFO, "plugins", "%u plugin file(s) in %s", static_cast<unsigned>(files.size()), directory.c_str());

	std::vector<wbl_plugin_s*> candidates;
	for (const std::string& f : files) {
		if (Discover(directory + f, f)) candidates.push_back(plugins_.back().get());
	}

	std::vector<host::PluginNode> nodes;
	for (wbl_plugin_s* p : candidates) {
		host::PluginNode node;
		node.id = p->id;
		const auto v = host::ParseVersion(p->version);
		node.version = v.value_or(host::Version{});
		for (const wbl_dependency& d : p->dependencies) {
			host::PluginDependency dep;
			dep.id = d.id;
			if (d.min_version != nullptr) dep.min_version = host::ParseVersion(d.min_version);
			dep.optional = (d.flags & WBL_DEPENDENCY_OPTIONAL) != 0;
			node.dependencies.push_back(dep);
		}
		nodes.push_back(std::move(node));
	}
	for (std::size_t i = 0; i < candidates.size(); ++i) {
		if (!host::ParseVersion(candidates[i]->version)) {
			Logf(WBL_LOG_WARN, "plugins", "%s: version '%s' is not major.minor.patch, treated as 0.0.0",
			     candidates[i]->id.c_str(), candidates[i]->version.c_str());
		}
	}

	const host::LoadPlan plan = host::PlanLoadOrder(nodes);
	for (const host::PluginRejection& r : plan.rejected) {
		Fail(*candidates[r.index], r.reason);
	}

	for (const std::size_t index : plan.order) {
		wbl_plugin_s& p = *candidates[index];
		std::string missing;
		for (const wbl_dependency& d : p.dependencies) {
			if ((d.flags & WBL_DEPENDENCY_OPTIONAL) != 0) continue;
			const wbl_plugin dep = Find(d.id);
			if (dep == nullptr || dep->status != wbl_plugin_s::Status::Loaded) {
				missing = d.id;
				break;
			}
		}
		if (!missing.empty()) {
			Fail(p, "dependency '" + missing + "' failed to load");
			continue;
		}

		wbl_result result = WBL_ERR_INTERNAL;
		if (!GuardedLoad(p.load, host, &p, &result)) {
			Fail(p, "wbl_plugin_load crashed");
			continue;
		}
		if (result != WBL_OK) {
			Fail(p, "wbl_plugin_load returned " + std::to_string(result));
			continue;
		}
		p.status = wbl_plugin_s::Status::Loaded;
		load_order_.push_back(&p);
		Logf(WBL_LOG_INFO, "plugins", "loaded %s %s (%s)", p.id.c_str(), p.version.c_str(), p.file_name.c_str());
	}
}

void PluginManager::UnloadAll() {
	for (auto it = load_order_.rbegin(); it != load_order_.rend(); ++it) {
		wbl_plugin_s& p = **it;
		if (p.unload != nullptr && !GuardedUnload(p.unload)) {
			Logf(WBL_LOG_ERROR, "plugins", "%s: wbl_plugin_unload crashed", p.id.c_str());
		}
		ReleaseOwner(&p);
		p.status = wbl_plugin_s::Status::Unloaded;
	}
	Sleep(100); // let threads leave detours that were just unlinked
	for (auto it = load_order_.rbegin(); it != load_order_.rend(); ++it) {
		if ((*it)->module != nullptr) {
			FreeLibrary((*it)->module);
			(*it)->module = nullptr;
		}
		Logf(WBL_LOG_INFO, "plugins", "unloaded %s", (*it)->id.c_str());
	}
	load_order_.clear();
}

wbl_plugin PluginManager::Find(const char* id) const {
	if (id == nullptr) return nullptr;
	for (const auto& p : plugins_) {
		if (p->status != wbl_plugin_s::Status::Failed && p->id == id) return p.get();
	}
	return nullptr;
}

bool PluginManager::Owns(wbl_plugin plugin) const {
	for (const auto& p : plugins_) {
		if (p.get() == plugin) return true;
	}
	return false;
}

std::vector<PluginSummary> PluginManager::Summaries() const {
	std::vector<PluginSummary> out;
	for (const auto& p : plugins_) {
		out.push_back({p->file_name, p->id, p->name, p->version, p->author, StatusText(p->status), p->error});
	}
	return out;
}

std::size_t PluginManager::LoadedCount() const { return load_order_.size(); }

} // namespace warbandlib::runtime
