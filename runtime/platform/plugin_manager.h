#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "warbandlib/warbandlib.h"

struct wbl_plugin_s {
	enum class Status { Discovered, Loaded, Failed, Unloaded };

	std::string file_name;
	std::string file_path;
	HMODULE module = nullptr;
	wbl_plugin_load_fn load = nullptr;
	wbl_plugin_unload_fn unload = nullptr;

	std::string id, name, version, author, description;
	std::vector<std::string> dependency_ids;
	std::vector<std::string> dependency_versions;
	std::vector<wbl_dependency> dependencies;
	wbl_plugin_info info{};

	Status status = Status::Discovered;
	std::string error;
};

namespace warbandlib::runtime {

struct PluginSummary {
	std::string file_name, id, name, version, author, status, error;
};

class PluginManager {
public:
	void LoadAll(const std::string& directory, const wbl_host_api* host);
	void UnloadAll();

	wbl_plugin Find(const char* id) const;
	bool Owns(wbl_plugin plugin) const;
	std::vector<PluginSummary> Summaries() const;
	std::size_t LoadedCount() const;

private:
	bool Discover(const std::string& path, const std::string& file_name);
	void Fail(wbl_plugin_s& plugin, const std::string& reason);

	std::vector<std::unique_ptr<wbl_plugin_s>> plugins_;
	std::vector<wbl_plugin_s*> load_order_;
};

} // namespace warbandlib::runtime
