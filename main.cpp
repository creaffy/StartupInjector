#include <windows.h>
#include <winternl.h>
#include <iostream>

#define error(f) printf("ERROR: " f " (%d)", GetLastError()); goto CLEANUP_FAIL;

int main(int argc, char** argv) {
	HMODULE hKernel32 = NULL;
	HANDLE hThread = NULL;
	LPVOID buffer = NULL;
	STARTUPINFOA si = { };
	PROCESS_INFORMATION pi = { };
	CHAR dllPath[MAX_PATH] = { };
	std::string commandLine = { };
	int code = 0;

	if (argc <= 2) {
		printf("Usage: SI.exe <exe_path> <dllPath> [args...]");
		goto CLEANUP_FAIL;
	}

	for (int i = 3; i < argc; i++) {
		commandLine.append(argv[i]).append(" ");
	}

	if (!CreateProcessA(argv[1], (LPSTR)commandLine.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
		error("CreateProcessA");
	}

	if (!GetFullPathNameA(argv[2], MAX_PATH, dllPath, NULL)) {
		error("GetFullPathNameA");
	}

	buffer = VirtualAllocEx(pi.hProcess, NULL, MAX_PATH, (MEM_COMMIT | MEM_RESERVE), PAGE_READWRITE);
	if (!buffer) {
		error("VirtualAllocEx");
	}

	if (!WriteProcessMemory(pi.hProcess, buffer, dllPath, MAX_PATH, NULL)) {
		error("WriteProcessMemory");
	}

	hKernel32 = GetModuleHandleA("kernel32");
	if (!hKernel32) {
		error("GetModuleHandleA");
	}

	hThread = CreateRemoteThread(pi.hProcess, NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(hKernel32, "LoadLibraryA")), buffer, 0, NULL);
	if (!hThread) {
		error("CreateRemoteThread");
	}

	if (WaitForSingleObject(hThread, INFINITE) == WAIT_FAILED) {
		error("WaitForSingleObject");
	}

	if (ResumeThread(pi.hThread) == -1) {
		error("ResumeThread");
	}

	goto CLEANUP_SUCCESS;

CLEANUP_FAIL:
	code = 1;

CLEANUP_SUCCESS:
	if (hThread) {
		CloseHandle(hThread);
	}

	if (pi.hProcess) {
		if (buffer) {
			VirtualFreeEx(pi.hProcess, buffer, 0, MEM_RELEASE);
		}
		CloseHandle(pi.hProcess);
	}

	if (pi.hThread) {
		CloseHandle(pi.hThread);
	}

	return code;
}