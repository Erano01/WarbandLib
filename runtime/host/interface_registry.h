#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#include "warbandlib/warbandlib.h"

namespace warbandlib::host {

class InterfaceRegistry {
public:
	wbl_result Register(const void* owner, const std::string& name, std::uint32_t version, const void* table);
	const void* Get(const std::string& name, std::uint32_t min_version, std::uint32_t* out_version) const;
	void RemoveOwner(const void* owner);

private:
	struct Entry {
		const void* owner;
		std::uint32_t version;
		const void* table;
	};
	mutable std::mutex mutex_;
	std::map<std::string, Entry> entries_;
};

} // namespace warbandlib::host
