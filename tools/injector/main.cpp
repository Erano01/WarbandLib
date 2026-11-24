// Standalone injector: finds a running process by name and loads/unloads a
// DLL in it via CreateRemoteThread + LoadLibraryA/FreeLibrary.
//
// Usage:
//   injector.exe <process_name.exe> <path\to\payload.dll>   (inject)
//   injector.exe --eject <process_name.exe> <dll_name.dll>  (unload)
//
// Eject asks the runtime to unload itself by creating <dll path>.unload and
// waits for the module to disappear: a remote FreeLibrary would run the
// shutdown under the loader lock, where plugins cannot be unloaded safely.

#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <string>

namespace {

DWORD FindProcessId(const std::string& process_name) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(entry);

	DWORD pid = 0;
	if (Process32First(snapshot, &entry)) {
		do {
			if (_stricmp(entry.szExeFile, process_name.c_str()) == 0) {
				pid = entry.th32ProcessID;
				break;
			}
		} while (Process32Next(snapshot, &entry));
	}

	CloseHandle(snapshot);
	return pid;
}

HMODULE FindRemoteModule(DWORD pid, const std::string& dll_name, std::string* out_path = nullptr) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return nullptr;
	}

	MODULEENTRY32 entry;
	entry.dwSize = sizeof(entry);

	HMODULE found = nullptr;
	if (Module32First(snapshot, &entry)) {
		do {
			if (_stricmp(entry.szModule, dll_name.c_str()) == 0) {
				found = entry.hModule;
				if (out_path != nullptr) {
					*out_path = entry.szExePath;
				}
				break;
			}
		} while (Module32Next(snapshot, &entry));
	}

	CloseHandle(snapshot);
	return found;
}

// Runs remote_fn(arg) in the target process via CreateRemoteThread and
// returns the thread's exit code, or false if any step failed.
bool RunRemoteThread(HANDLE process, LPTHREAD_START_ROUTINE remote_fn, LPVOID arg,
                      DWORD* out_exit_code) {
	HANDLE remote_thread = CreateRemoteThread(process, nullptr, 0, remote_fn, arg, 0, nullptr);
	if (remote_thread == nullptr) {
		std::fprintf(stderr, "CreateRemoteThread failed: %lu\n", GetLastError());
		return false;
	}

	WaitForSingleObject(remote_thread, INFINITE);
	GetExitCodeThread(remote_thread, out_exit_code);
	CloseHandle(remote_thread);
	return true;
}

bool InjectDll(DWORD pid, const std::string& dll_path) {
	HANDLE process = OpenProcess(
	    PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
	        PROCESS_VM_WRITE | PROCESS_VM_READ,
	    FALSE, pid);
	if (process == nullptr) {
		std::fprintf(stderr, "OpenProcess failed: %lu\n", GetLastError());
		return false;
	}

	const std::size_t path_size = dll_path.size() + 1;
	LPVOID remote_path = VirtualAllocEx(process, nullptr, path_size, MEM_COMMIT, PAGE_READWRITE);
	if (remote_path == nullptr) {
		std::fprintf(stderr, "VirtualAllocEx failed: %lu\n", GetLastError());
		CloseHandle(process);
		return false;
	}

	if (!WriteProcessMemory(process, remote_path, dll_path.c_str(), path_size, nullptr)) {
		std::fprintf(stderr, "WriteProcessMemory failed: %lu\n", GetLastError());
		VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
		CloseHandle(process);
		return false;
	}

	auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
	    reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA")));
	if (load_library == nullptr) {
		std::fprintf(stderr, "GetProcAddress(LoadLibraryA) failed: %lu\n", GetLastError());
		VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
		CloseHandle(process);
		return false;
	}

	DWORD loaded_module = 0;
	const bool ran = RunRemoteThread(process, load_library, remote_path, &loaded_module);

	VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
	CloseHandle(process);

	if (!ran) {
		return false;
	}
	if (loaded_module == 0) {
		std::fprintf(stderr, "Remote LoadLibraryA returned NULL -- DLL failed to load\n");
		return false;
	}
	return true;
}

bool EjectDll(DWORD pid, const std::string& dll_name) {
	std::string dll_path;
	if (FindRemoteModule(pid, dll_name, &dll_path) == nullptr) {
		std::fprintf(stderr, "'%s' is not currently loaded in that process\n", dll_name.c_str());
		return false;
	}

	const std::string flag = dll_path + ".unload";
	const HANDLE file = CreateFileA(flag.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) {
		std::fprintf(stderr, "could not create %s: %lu\n", flag.c_str(), GetLastError());
		return false;
	}
	CloseHandle(file);

	for (int i = 0; i < 80; ++i) {
		Sleep(250);
		if (FindRemoteModule(pid, dll_name) == nullptr) {
			return true;
		}
	}
	std::fprintf(stderr, "'%s' is still loaded after 20 s\n", dll_name.c_str());
	return false;
}

} // namespace

int main(int argc, char** argv) {
	bool eject_mode = false;
	int arg_offset = 1;
	if (argc >= 2 && std::string(argv[1]) == "--eject") {
		eject_mode = true;
		arg_offset = 2;
	}

	if (argc != arg_offset + 2) {
		std::fprintf(stderr,
		              "Usage:\n"
		              "  %s <process_name.exe> <path\\to\\payload.dll>   (inject)\n"
		              "  %s --eject <process_name.exe> <dll_name.dll>  (unload)\n",
		              argv[0], argv[0]);
		return 1;
	}

	const std::string process_name = argv[arg_offset];
	const std::string dll_arg = argv[arg_offset + 1];

	const DWORD pid = FindProcessId(process_name);
	if (pid == 0) {
		std::fprintf(stderr, "Process '%s' not found\n", process_name.c_str());
		return 1;
	}

	if (eject_mode) {
		std::printf("Found %s (pid %lu), ejecting %s...\n", process_name.c_str(), pid,
		            dll_arg.c_str());
		if (!EjectDll(pid, dll_arg)) {
			return 1;
		}
		std::printf("Ejected successfully.\n");
		return 0;
	}

	std::printf("Found %s (pid %lu), injecting %s...\n", process_name.c_str(), pid,
	            dll_arg.c_str());
	if (!InjectDll(pid, dll_arg)) {
		return 1;
	}
	std::printf("Injected successfully.\n");
	return 0;
}
