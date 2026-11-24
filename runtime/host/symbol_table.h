#pragma once

#include <cstdint>
#include <istream>
#include <map>
#include <optional>
#include <string>

namespace warbandlib::host {

// Per-build game data: an ini whose sections are named after the exe's SHA-256 and whose
// entries are `name = value` (hex 0x.. or decimal). The data itself is not part of WarbandLib.
class SymbolTable {
public:
	static std::optional<SymbolTable> Parse(std::istream& in, const std::string& exe_sha256_hex);

	std::optional<std::uint32_t> Get(const std::string& name) const;
	bool Has(const std::string& name) const { return Get(name).has_value(); }
	std::size_t Size() const { return values_.size(); }

private:
	std::map<std::string, std::uint32_t> values_;
};

} // namespace warbandlib::host
