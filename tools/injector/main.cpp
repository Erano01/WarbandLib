// Standalone injector: finds a running process by name and loads a DLL into
// it via the classic CreateRemoteThread + LoadLibraryA technique.
//
// Usage: injector.exe <process_name.exe> <path\to\payload.dll>

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

	HANDLE remote_thread =
	    CreateRemoteThread(process, nullptr, 0, load_library, remote_path, 0, nullptr);
	if (remote_thread == nullptr) {
		std::fprintf(stderr, "CreateRemoteThread failed: %lu\n", GetLastError());
		VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
		CloseHandle(process);
		return false;
	}

	WaitForSingleObject(remote_thread, INFINITE);

	DWORD loaded_module = 0;
	GetExitCodeThread(remote_thread, &loaded_module);

	CloseHandle(remote_thread);
	VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
	CloseHandle(process);

	if (loaded_module == 0) {
		std::fprintf(stderr, "Remote LoadLibraryA returned NULL -- DLL failed to load\n");
		return false;
	}
	return true;
}

} // namespace

int main(int argc, char** argv) {
	if (argc != 3) {
		std::fprintf(stderr, "Usage: %s <process_name.exe> <path\\to\\payload.dll>\n", argv[0]);
		return 1;
	}

	const std::string process_name = argv[1];
	const std::string dll_path = argv[2];

	const DWORD pid = FindProcessId(process_name);
	if (pid == 0) {
		std::fprintf(stderr, "Process '%s' not found\n", process_name.c_str());
		return 1;
	}

	std::printf("Found %s (pid %lu), injecting %s...\n", process_name.c_str(), pid,
	            dll_path.c_str());

	if (!InjectDll(pid, dll_path)) {
		return 1;
	}

	std::printf("Injected successfully.\n");
	return 0;
}
