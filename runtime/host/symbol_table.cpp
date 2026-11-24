#include "runtime/host/symbol_table.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace warbandlib::host {

namespace {

std::string Trim(const std::string& s) {
	const auto b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return {};
	const auto e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

std::string Lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::uint32_t> ParseNumber(const std::string& text) {
	if (text.empty()) return std::nullopt;
	char* end = nullptr;
	const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
	const unsigned long long v = std::strtoull(text.c_str(), &end, hex ? 16 : 10);
	if (end == nullptr || *end != '\0' || v > 0xFFFFFFFFull) return std::nullopt;
	return static_cast<std::uint32_t>(v);
}

} // namespace

std::optional<SymbolTable> SymbolTable::Parse(std::istream& in, const std::string& exe_sha256_hex) {
	const std::string wanted = "[" + Lower(exe_sha256_hex) + "]";
	SymbolTable table;
	bool in_section = false, found = false;
	std::string line;
	while (std::getline(in, line)) {
		std::string t = Trim(line);
		const auto comment = t.find_first_of(";#");
		if (comment != std::string::npos) t = Trim(t.substr(0, comment));
		if (t.empty()) continue;
		if (t.front() == '[' && t.back() == ']') {
			in_section = Lower(t) == wanted;
			found = found || in_section;
			continue;
		}
		if (!in_section) continue;
		const auto eq = t.find('=');
		if (eq == std::string::npos) continue;
		const auto value = ParseNumber(Trim(t.substr(eq + 1)));
		if (value) table.values_[Lower(Trim(t.substr(0, eq)))] = *value;
	}
	if (!found) return std::nullopt;
	return table;
}

std::optional<std::uint32_t> SymbolTable::Get(const std::string& name) const {
	const auto it = values_.find(Lower(name));
	if (it == values_.end()) return std::nullopt;
	return it->second;
}

} // namespace warbandlib::host
