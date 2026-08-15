#include <windows.h>

#include <atomic>
#include <memory>

#include "core/logging.h"
#include "core/win32/module.h"
#include "runtime/graphics/d3d9/bootstrap.h"
#include "runtime/graphics/d3d9/endscene_hook.h"

namespace {

using warbandlib::core::Logger;
using warbandlib::runtime::graphics::d3d9::BootstrapEndSceneHook;
using warbandlib::runtime::graphics::d3d9::EndSceneHook;

// Fingerprint of the exe this hook is written against. See
// re/mb_warband.exe.md and signatures/mb_warband.ini.
constexpr const char* kExpectedFingerprint =
    "ff4a28c51bed49fe4bbb4bb96389f7611894f6d031bf68fff661efdc946dae13";

HMODULE g_this_module = nullptr;
std::atomic<bool> g_shutdown{false};
HANDLE g_worker_thread = nullptr;

std::unique_ptr<Logger> g_logger;
std::unique_ptr<EndSceneHook> g_hook;

DWORD WINAPI WorkerThread(LPVOID) {
	const std::string dir = warbandlib::core::win32::GetModuleDirectory(g_this_module);
	g_logger = std::make_unique<Logger>(dir + "WarbandLib.log");
	g_logger->Write("WarbandLib runtime attached, waiting for D3D device...");

	g_hook = std::make_unique<EndSceneHook>(g_logger.get());
	BootstrapEndSceneHook(g_this_module, kExpectedFingerprint, *g_logger, *g_hook, g_shutdown);
	return 0;
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
