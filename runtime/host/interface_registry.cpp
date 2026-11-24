#include "runtime/host/interface_registry.h"

namespace warbandlib::host {

wbl_result InterfaceRegistry::Register(const void* owner, const std::string& name, std::uint32_t version,
                                       const void* table) {
	if (name.empty() || table == nullptr) {
		return WBL_ERR_INVALID_ARGUMENT;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = entries_.find(name);
	if (it != entries_.end()) {
		if (it->second.owner != owner) {
			return WBL_ERR_ALREADY_EXISTS;
		}
		it->second = Entry{owner, version, table};
		return WBL_OK;
	}
	entries_.emplace(name, Entry{owner, version, table});
	return WBL_OK;
}

const void* InterfaceRegistry::Get(const std::string& name, std::uint32_t min_version, std::uint32_t* out_version) const {
	std::lock_guard<std::mutex> lock(mutex_);
	const auto it = entries_.find(name);
	if (it == entries_.end() || it->second.version < min_version) {
		return nullptr;
	}
	if (out_version != nullptr) {
		*out_version = it->second.version;
	}
	return it->second.table;
}

void InterfaceRegistry::RemoveOwner(const void* owner) {
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto it = entries_.begin(); it != entries_.end();) {
		it = it->second.owner == owner ? entries_.erase(it) : std::next(it);
	}
}

} // namespace warbandlib::host
