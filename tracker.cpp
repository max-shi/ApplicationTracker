#include "tracker.h"
#include "database.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>

// Global variables to store the last active window details and session id.
static std::string lastProcessName = "";
static std::string lastWindowTitle = "";
static int currentSessionId = 0; // 0 indicates no active session.

// Helper function to retrieve active window details.
void getActiveWindowDetails(std::string & processName, std::string & windowTitle) {
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        processName = "";
        windowTitle = "";
        return;
    }

    char titleBuffer[256] = { 0 };
    GetWindowTextA(foregroundWindow, titleBuffer, sizeof(titleBuffer));
    windowTitle = titleBuffer;

    DWORD processID = 0;
    GetWindowThreadProcessId(foregroundWindow, &processID);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    char processBuffer[MAX_PATH] = "<unknown>";
    if (hProcess) {
        HMODULE hMod;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            GetModuleBaseNameA(hProcess, hMod, processBuffer, sizeof(processBuffer));
        }
        CloseHandle(hProcess);
    }
    processName = processBuffer;
}

void trackActiveWindowSession() {
    std::string currentProcessName;
    std::string currentWindowTitle;
    getActiveWindowDetails(currentProcessName, currentWindowTitle);

    // Check if the active window has changed.
    if (currentProcessName != lastProcessName || currentWindowTitle != lastWindowTitle) {
        // End the previous session if one exists.
        if (!lastProcessName.empty() || !lastWindowTitle.empty()) {
            if (currentSessionId != 0) {
                if (!endSession(currentSessionId)) {
                    std::cerr << "Failed to end session for " << lastProcessName
                              << " - " << lastWindowTitle << std::endl;
                }
            }
        }

        // Start a new session if the current details are not empty.
        if (!currentProcessName.empty() && !currentWindowTitle.empty()) {
            if (!startSession(currentProcessName, currentWindowTitle, currentSessionId)) {
                std::cerr << "Failed to start new session for " << currentProcessName
                          << " - " << currentWindowTitle << std::endl;
            } else {
                std::cout << "New session started: " << currentProcessName
                          << " | " << currentWindowTitle << std::endl;
            }
        }

        // Update the last active window details.
        lastProcessName = currentProcessName;
        lastWindowTitle = currentWindowTitle;
    }
}
