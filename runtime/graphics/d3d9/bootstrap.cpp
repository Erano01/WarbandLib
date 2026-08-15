#include "runtime/graphics/d3d9/bootstrap.h"

#include "core/win32/module.h"

namespace warbandlib::runtime::graphics::d3d9 {

namespace {
constexpr DWORD kPollIntervalMs = 250;
constexpr int kMaxPollAttempts = 4 * 60 * 5; // ~5 minutes at 250ms
} // namespace

bool BootstrapEndSceneHook(HMODULE this_module, const char* expected_fingerprint,
                            core::Logger& logger, EndSceneHook& hook,
                            const std::atomic<bool>& shutdown_requested) {
	const std::string dir = core::win32::GetModuleDirectory(this_module);

	const std::optional<core::win32::SignatureData> signature =
	    core::win32::LoadSignatureData(dir + "mb_warband.ini", expected_fingerprint);
	if (!signature.has_value()) {
		logger.Write("ERROR: no signature entry for this exe fingerprint, aborting");
		return false;
	}

	const std::optional<core::win32::Module> module =
	    core::win32::Module::ResolveMainModule(expected_fingerprint);
	if (!module.has_value()) {
		logger.Write("ERROR: main module fingerprint mismatch, aborting");
		return false;
	}

	for (int attempt = 0; attempt < kMaxPollAttempts && !shutdown_requested.load(); ++attempt) {
		if (hook.TryInstall(*module, *signature)) {
			return true;
		}
		Sleep(kPollIntervalMs);
	}

	logger.Write("ERROR: device pointer never became available, giving up");
	return false;
}

} // namespace warbandlib::runtime::graphics::d3d9
