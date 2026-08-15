#include "core/win32/module.h"

#include <windows.h>

#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

#include "core/sha256.h"

namespace warbandlib::core::win32 {

namespace {

std::string Trim(const std::string& s) {
	std::size_t begin = 0;
	while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) ++begin;
	std::size_t end = s.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
	return s.substr(begin, end - begin);
}

std::string ToLower(std::string s) {
	for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

std::uintptr_t ParseHexOrDec(const std::string& value) {
	return static_cast<std::uintptr_t>(std::stoull(value, nullptr, 0));
}

} // namespace

std::string GetModuleDirectory(HMODULE module_handle) {
	char path[MAX_PATH];
	const DWORD len = GetModuleFileNameA(module_handle, path, MAX_PATH);
	if (len == 0 || len == MAX_PATH) {
		return {};
	}
	const std::string s(path, len);
	const std::size_t slash = s.find_last_of("\\/");
	return slash == std::string::npos ? std::string{} : s.substr(0, slash + 1);
}

std::optional<SignatureData> LoadSignatureData(const std::string& ini_path,
                                                const std::string& exe_fingerprint_hex) {
	std::ifstream in(ini_path);
	if (!in.is_open()) {
		return std::nullopt;
	}

	const std::string target_section = "[" + ToLower(exe_fingerprint_hex) + "]";

	std::string line;
	bool in_target_section = false;
	SignatureData data;
	bool found_section = false;

	while (std::getline(in, line)) {
		const std::string trimmed = Trim(line);
		if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
			continue;
		}

		if (trimmed.front() == '[' && trimmed.back() == ']') {
			in_target_section = (ToLower(trimmed) == target_section);
			if (in_target_section) {
				found_section = true;
			}
			continue;
		}

		if (!in_target_section) {
			continue;
		}

		const std::size_t eq = trimmed.find('=');
		if (eq == std::string::npos) {
			continue;
		}

		const std::string key = ToLower(Trim(trimmed.substr(0, eq)));
		const std::string value = Trim(trimmed.substr(eq + 1));

		if (key == "g_app_instance") {
			data.app_instance = ParseHexOrDec(value);
		} else if (key == "g_app_device_offset") {
			data.app_device_offset = ParseHexOrDec(value);
		} else if (key == "d3d9_device_vtbl_endscene_index") {
			data.endscene_vtbl_index = static_cast<std::size_t>(ParseHexOrDec(value));
		}
	}

	if (!found_section) {
		return std::nullopt;
	}
	return data;
}

std::optional<Module> Module::ResolveMainModule(const std::string& expected_fingerprint_hex) {
	const HMODULE main_module = GetModuleHandleA(nullptr);
	if (main_module == nullptr) {
		return std::nullopt;
	}

	char exe_path[MAX_PATH];
	const DWORD path_len = GetModuleFileNameA(main_module, exe_path, MAX_PATH);
	if (path_len == 0 || path_len == MAX_PATH) {
		return std::nullopt;
	}

	std::ifstream file(exe_path, std::ios::binary);
	if (!file.is_open()) {
		return std::nullopt;
	}
	std::vector<std::uint8_t> contents((std::istreambuf_iterator<char>(file)),
	                                    std::istreambuf_iterator<char>());
	file.close();

	const std::string actual_fingerprint = Sha256Hex(contents.data(), contents.size());
	if (ToLower(actual_fingerprint) != ToLower(expected_fingerprint_hex)) {
		return std::nullopt;
	}

	return Module(reinterpret_cast<std::uintptr_t>(main_module));
}

Module::Module(std::uintptr_t base_address) : base_address_(base_address) {}

void* Module::Va(std::uintptr_t static_analysis_va) const {
	const std::uintptr_t rva = static_analysis_va - kStaticAnalysisImageBase;
	return reinterpret_cast<void*>(base_address_ + rva);
}

} // namespace warbandlib::core::win32
