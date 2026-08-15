#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace warbandlib::core::win32 {

// Raw offset/index data loaded from a signatures/*.ini entry, keyed by the
// target exe's SHA-256 fingerprint. See signatures/mb_warband.ini.
struct SignatureData {
	std::uintptr_t app_instance = 0;
	std::uintptr_t app_device_offset = 0;
	std::size_t endscene_vtbl_index = 0;
};

// Parses an ini file with sections named after a lowercase hex SHA-256
// fingerprint, and returns the entry matching exe_fingerprint_hex.
// Returns std::nullopt if the file can't be read or no section matches.
std::optional<SignatureData> LoadSignatureData(const std::string& ini_path,
                                                const std::string& exe_fingerprint_hex);

// Represents the main module (mb_warband.exe) loaded into the current
// process. Resolves its base address and validates it against a known-good
// fingerprint before any offset from SignatureData is trusted.
class Module {
public:
	// Hashes the on-disk exe backing the main module and compares it against
	// expected_fingerprint_hex. Returns nullopt if the module can't be
	// resolved, read, or the fingerprint doesn't match.
	static std::optional<Module> ResolveMainModule(const std::string& expected_fingerprint_hex);

	// Translates an absolute VA recorded during static analysis (against a
	// non-ASLR build with base 0x00400000) into the VA to use at runtime.
	void* Va(std::uintptr_t static_analysis_va) const;

private:
	explicit Module(std::uintptr_t base_address);

	std::uintptr_t base_address_;
	static constexpr std::uintptr_t kStaticAnalysisImageBase = 0x00400000;
};

} // namespace warbandlib::core::win32
