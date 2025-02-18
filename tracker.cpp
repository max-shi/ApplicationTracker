#include "tracker.h"
#include "database.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>

void trackActiveWindow() {
    // Get the handle of the foreground (active) window
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow)
        return;

    // Retrieve the window title
    char windowTitle[256] = { 0 };
    GetWindowTextA(foregroundWindow, windowTitle, sizeof(windowTitle));

    // Get the process ID associated with this window
    DWORD processID = 0;
    GetWindowThreadProcessId(foregroundWindow, &processID);

    // Open the process to query information
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    char processName[MAX_PATH] = "<unknown>";
    if (hProcess) {
        HMODULE hMod;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            GetModuleBaseNameA(hProcess, hMod, processName, sizeof(processName));
        }
        CloseHandle(hProcess);
    }

    // Log the captured data into the database
    if (!logActivity(std::string(processName), std::string(windowTitle))) {
        std::cerr << "Failed to log activity" << std::endl;
    }
}
