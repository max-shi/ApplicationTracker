#include "tracker.h"
#include "database.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>
// test
// Global variables to store the last active window details and session id.
static std::string lastProcessName = "";
static std::string lastWindowTitle = "";
static int currentSessionId = 0; // 0 indicates no active session.

// Idle threshold in milliseconds (e.g., 5 minutes = 300000 ms).
const DWORD idleThreshold = 300000;

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

// Helper function to get idle time in milliseconds using Windows API.
DWORD getIdleTime() {
    LASTINPUTINFO lastInput;
    lastInput.cbSize = sizeof(LASTINPUTINFO);
    if (!GetLastInputInfo(&lastInput))
        return 0;
    return GetTickCount() - lastInput.dwTime;
}

void trackActiveWindowSession() {
    // First, check if the user is idle.
    DWORD idleTime = getIdleTime();
    if (idleTime > idleThreshold) {
        // If the user is idle and a session is active, end it.
        if (currentSessionId != 0) {
            if (!endSession(currentSessionId)) {
                std::cerr << "Failed to end session due to idle state for "
                          << lastProcessName << " - " << lastWindowTitle << std::endl;
            } else {
                std::cout << "Session ended due to idle state." << std::endl;
            }
            currentSessionId = 0;
            lastProcessName = "";
            lastWindowTitle = "";
        }
        // Skip further processing if the user is idle.
        return;
    }

    // If user is active, continue tracking.
    std::string currentProcessName;
    std::string currentWindowTitle;
    getActiveWindowDetails(currentProcessName, currentWindowTitle);

    // Check if the active window has changed.
    if (currentProcessName != lastProcessName || currentWindowTitle != lastWindowTitle) {
        // End the previous session if one exists.
        if (!lastProcessName.empty() || !lastWindowTitle.empty()) {
            if (currentSessionId != 0) {
                if (!endSession(currentSessionId)) {
                    std::cerr << "Failed to end session for "
                              << lastProcessName << " - " << lastWindowTitle << std::endl;
                }
            }
        }

        // Start a new session if the current details are not empty.
        if (!currentProcessName.empty() && !currentWindowTitle.empty()) {
            if (!startSession(currentProcessName, currentWindowTitle, currentSessionId)) {
                std::cerr << "Failed to start new session for "
                          << currentProcessName << " - " << currentWindowTitle << std::endl;
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
