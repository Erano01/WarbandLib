#include <windows.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "core/sha256.h"
#include "core/win32/module.h"
#include "runtime/platform/overlay.h"
#include "runtime/platform/runtime_state.h"

namespace {

using namespace warbandlib::runtime;

HMODULE g_self = nullptr;
bool g_shut_down = false;

std::string ModulePath(HMODULE module) {
	char path[MAX_PATH] = {};
	const DWORD len = GetModuleFileNameA(module, path, MAX_PATH);
	return std::string(path, len);
}

void FillGameInfo(RuntimeState& s) {
	wbl_game_info& g = s.game_info;
	g.struct_size = sizeof(g);
	const HMODULE exe = GetModuleHandleA(nullptr);
	g.image_base = reinterpret_cast<uintptr_t>(exe);
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const char*>(exe) + dos->e_lfanew);
	g.image_size = nt->OptionalHeader.SizeOfImage;

	std::ifstream file(ModulePath(exe), std::ios::binary);
	const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	const std::string sha = warbandlib::core::Sha256Hex(bytes.data(), bytes.size());
	std::snprintf(g.sha256, sizeof(g.sha256), "%s", sha.c_str());
	g.fingerprint_known =
	    warbandlib::core::win32::LoadSignatureData(s.runtime_dir + "mb_warband.ini", sha).has_value() ? WBL_TRUE : WBL_FALSE;
	Logf(WBL_LOG_INFO, "runtime", "mb_warband.exe base=0x%08X size=0x%X sha256=%s (%s)", static_cast<unsigned>(g.image_base),
	     g.image_size, g.sha256, g.fingerprint_known ? "known build" : "unknown build");
}

void Shutdown(bool from_worker) {
	if (g_shut_down) return;
	g_shut_down = true;
	RuntimeState& s = State();
	Log(WBL_LOG_INFO, "runtime", "shutting down");
	overlay::Uninstall();
	if (!from_worker) {
		// Loader lock is held: no waiting, no FreeLibrary. Only detach from the game.
		s.hooks.Shutdown();
		return;
	}
	Sleep(300); // render thread leaves Present
	s.events.Post(WBL_EVENT_SHUTDOWN, nullptr);
	s.plugins.UnloadAll();
	s.hooks.Shutdown();
	overlay::Shutdown();
	Log(WBL_LOG_INFO, "runtime", "shut down cleanly");
}

DWORD WINAPI Worker(LPVOID) {
	RuntimeState& s = State();
	const std::string self_path = ModulePath(g_self);
	s.runtime_dir = self_path.substr(0, self_path.find_last_of("\\/") + 1);
	s.plugin_dir = s.runtime_dir + "plugins\\";
	DeleteFileA((self_path + ".log").c_str());
	s.logger = std::make_unique<warbandlib::core::Logger>(self_path + ".log");

	char mutex_name[64];
	std::snprintf(mutex_name, sizeof(mutex_name), "WarbandLib_Runtime_%lu", GetCurrentProcessId());
	HANDLE instance = CreateMutexA(nullptr, FALSE, mutex_name);
	if (instance == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
		Log(WBL_LOG_WARN, "runtime", "another WarbandLib runtime is already loaded in this process, exiting");
		if (instance != nullptr) CloseHandle(instance);
		g_shut_down = true;
		FreeLibraryAndExitThread(g_self, 0);
	}

	Logf(WBL_LOG_INFO, "runtime", "WarbandLib %s (ABI %u.%u) at %s", kRuntimeVersion, WBL_ABI_VERSION_MAJOR,
	     WBL_ABI_VERSION_MINOR, self_path.c_str());
	FillGameInfo(s);
	if (!s.hooks.Initialize()) {
		Log(WBL_LOG_ERROR, "runtime", "MinHook initialisation failed, plugin hooks unavailable");
	}
	CreateDirectoryA(s.plugin_dir.c_str(), nullptr);
	s.plugins.LoadAll(s.plugin_dir, HostApi());
	overlay::Install(g_self);
	s.events.Post(WBL_EVENT_PLUGINS_LOADED, nullptr);
	Logf(WBL_LOG_INFO, "runtime", "ready: %u plugin(s) loaded", static_cast<unsigned>(s.plugins.LoadedCount()));

	const std::string unload_flag = self_path + ".unload";
	while (GetFileAttributesA(unload_flag.c_str()) == INVALID_FILE_ATTRIBUTES) {
		Sleep(250);
	}
	DeleteFileA(unload_flag.c_str());
	Shutdown(true);
	s.logger.reset();
	CloseHandle(instance);
	FreeLibraryAndExitThread(g_self, 0);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
	if (reason == DLL_PROCESS_ATTACH) {
		g_self = module;
		DisableThreadLibraryCalls(module);
		const HANDLE thread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
		if (thread != nullptr) CloseHandle(thread);
	} else if (reason == DLL_PROCESS_DETACH && reserved == nullptr) {
		Shutdown(false);
	}
	return TRUE;
}
