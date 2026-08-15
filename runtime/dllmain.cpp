#include <windows.h>

#include <atomic>
#include <memory>
#include <string>

#include "core/logging.h"
#include "core/win32/module.h"
#include "runtime/graphics/d3d9/endscene_hook.h"

namespace {

using warbandlib::core::Logger;
using warbandlib::core::win32::LoadSignatureData;
using warbandlib::core::win32::Module;
using warbandlib::core::win32::SignatureData;
using warbandlib::runtime::graphics::d3d9::EndSceneHook;

// Fingerprint of the exe this hook is written against. See
// re/mb_warband.exe.md and signatures/mb_warband.ini.
constexpr const char* kExpectedFingerprint =
    "ff4a28c51bed49fe4bbb4bb96389f7611894f6d031bf68fff661efdc946dae13";

constexpr DWORD kPollIntervalMs = 250;
constexpr int kMaxPollAttempts = 4 * 60 * 5; // ~5 minutes at 250ms

HMODULE g_this_module = nullptr;
std::atomic<bool> g_shutdown{false};
HANDLE g_worker_thread = nullptr;

std::unique_ptr<Logger> g_logger;
std::unique_ptr<EndSceneHook> g_hook;

std::string ModuleDirectory() {
	char path[MAX_PATH];
	const DWORD len = GetModuleFileNameA(g_this_module, path, MAX_PATH);
	if (len == 0 || len == MAX_PATH) {
		return {};
	}
	std::string s(path, len);
	const std::size_t slash = s.find_last_of("\\/");
	return slash == std::string::npos ? std::string{} : s.substr(0, slash + 1);
}

DWORD WINAPI WorkerThread(LPVOID) {
	const std::string dir = ModuleDirectory();
	g_logger = std::make_unique<Logger>(dir + "WarbandLib.log");
	g_logger->Write("WarbandLib runtime attached, waiting for D3D device...");

	const std::optional<SignatureData> signature =
	    LoadSignatureData(dir + "mb_warband.ini", kExpectedFingerprint);
	if (!signature.has_value()) {
		g_logger->Write("ERROR: no signature entry for this exe fingerprint, aborting");
		return 1;
	}

	std::optional<Module> module = Module::ResolveMainModule(kExpectedFingerprint);
	if (!module.has_value()) {
		g_logger->Write("ERROR: main module fingerprint mismatch, aborting");
		return 1;
	}

	g_hook = std::make_unique<EndSceneHook>(g_logger.get());

	for (int attempt = 0; attempt < kMaxPollAttempts && !g_shutdown.load(); ++attempt) {
		if (g_hook->TryInstall(*module, *signature)) {
			return 0;
		}
		Sleep(kPollIntervalMs);
	}

	g_logger->Write("ERROR: device pointer never became available, giving up");
	return 1;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module_handle, DWORD reason, LPVOID) {
	switch (reason) {
		case DLL_PROCESS_ATTACH:
			g_this_module = module_handle;
			DisableThreadLibraryCalls(module_handle);
			g_worker_thread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
			break;

		case DLL_PROCESS_DETACH:
			g_shutdown.store(true);
			if (g_worker_thread != nullptr) {
				WaitForSingleObject(g_worker_thread, 2000);
				CloseHandle(g_worker_thread);
				g_worker_thread = nullptr;
			}
			g_hook.reset(); // uninstalls the vtable hook
			g_logger.reset();
			break;

		default:
			break;
	}
	return TRUE;
}
