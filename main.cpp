// Include core ImGui functionality.
// including backends and gl3w (API functions et c)
#include <chrono>
#include <database.h>
#include <imgui_internal.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include <GL/gl3w.h>
#include <windows.h>
#include <tchar.h>
#include <thread>
#include <tracker.h>
#include "pie_chart.h"
#include "functions.h"

#include <cstdio>   // for snprintf, sscanf
#include <ctime>    // for std::tm, mktime

static int mode = 0; // 0 = All-time, 1 = Daily average
static char selectedDate[11];  // Default date in YYYY-MM-DD format
// Define an idle threshold (e.g., 5 minutes = 300000 ms)
const DWORD idleThreshold = 300000;
// Global or static variable to keep track of the current session ID.
static int currentSessionId = -1;
static bool sessionActive = false;

// Forward declaration of ImGui's Win32 message handler (defined in imgui_impl_win32.cpp).
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//-----------------------------------------------------------------------------
// Win32 window procedure: Handles window messages such as resizing and closing.
//-----------------------------------------------------------------------------
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // First, let ImGui handle the message.
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true; // Return if ImGui has already handled the message.

    // Process messages that ImGui does not handle.
    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            glViewport(0, 0, width, height);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void initializeCurrentDate() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    std::strftime(selectedDate, sizeof(selectedDate), "%Y-%m-%d", &tm);
}

// --- Calendar View Implementation ---
// Helper: Get the number of days in a given month.
int GetDaysInMonth(int year, int month) {
    if (month == 2) {
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11)
        return 30;
    return 31;
}

// Draws a simple calendar view. When a day is clicked the global selectedDate is updated.
void DrawCalendar() {
    // Static calendar state; initialize from selectedDate on first run.
    static int calYear = 0, calMonth = 0, calDay = 0;
    if (calYear == 0) {
        sscanf(selectedDate, "%d-%d-%d", &calYear, &calMonth, &calDay);
    }

    // Month navigation buttons.
    if (ImGui::Button("<")) {
        mode = 1;
        calMonth--;
        if (calMonth < 1) {
            calMonth = 12;
            calYear--;
        }
        calDay = 0; // Reset day selection
    }
    ImGui::SameLine();
    {
        char header[32];
        snprintf(header, sizeof(header), "%04d-%02d", calYear, calMonth);
        ImGui::Text("%s", header);
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        mode = 1;
        calMonth++;
        if (calMonth > 12) {
            calMonth = 1;
            calYear++;
        }
        calDay = 0;
    }

    ImGui::Spacing();

    // Display days-of-week header.
    const char* daysOfWeek[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    for (int i = 0; i < 7; i++) {
        ImGui::SameLine(i * 50.0f); // Adjust spacing as needed.
        ImGui::Text("%s", daysOfWeek[i]);
    }
    ImGui::NewLine();
    ImGui::Separator();

    // Determine first weekday of the month.
    std::tm time_in = {};
    time_in.tm_year = calYear - 1900;
    time_in.tm_mon = calMonth - 1;
    time_in.tm_mday = 1;
    mktime(&time_in);
    int firstWeekday = time_in.tm_wday; // Sunday = 0, Monday = 1, etc.

    int daysInMonth = GetDaysInMonth(calYear, calMonth);
    int cellWidth = 40;
    int cellHeight = 40;
    int col = 0;

    // Leave empty cells until the first day.
    for (int i = 0; i < firstWeekday; i++) {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(cellWidth, cellHeight));
        col++;
    }

    // Render each day as a button.
    for (int day = 1; day <= daysInMonth; day++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", day);

        bool push = (day == calDay);
        if (push) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.41f, 0.41f, 0.41f, 1.00f)); // green highlight
        }
        if (ImGui::Button(buf, ImVec2(cellWidth, cellHeight))) {
            calDay = day;
            mode = 1;
            // Update the global selectedDate in YYYY-MM-DD format.
            snprintf(selectedDate, sizeof(selectedDate), "%04d-%02d-%02d", calYear, calMonth, calDay);
        }
        if (push) {
            ImGui::PopStyleColor();
        }

        col++;
        if (col % 7 != 0)
            ImGui::SameLine();
        else
            ImGui::NewLine();
    }

    ImGui::Spacing();
    if (calDay != 0) {
        char selectedBuf[32];
        snprintf(selectedBuf, sizeof(selectedBuf), "Selected Date: %04d-%02d-%02d", calYear, calMonth, calDay);
        ImGui::Text("%s", selectedBuf);
    }
}

// --- End Calendar View Implementation ---

void load_ImGui() {
    // --- Controls Pane ---
    ImGui::Begin("Controls");

    // Date input field.
    ImGui::InputText("Date (YYYY-MM-DD)", selectedDate, sizeof(selectedDate));
    // Mode selection buttons.
    if (ImGui::Button("All-Time")) { mode = 0; }
    ImGui::SameLine();
    if (ImGui::Button("Day Total")) { mode = 1; }
    ImGui::SameLine();
    if (ImGui::Button("<")) {
        std::string newDate = getPreviousDate(std::string(selectedDate));
        strncpy(selectedDate, newDate.c_str(), sizeof(selectedDate));
        selectedDate[sizeof(selectedDate)-1] = '\0';
        mode = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        std::string newDate = getNextDate(std::string(selectedDate));
        strncpy(selectedDate, newDate.c_str(), sizeof(selectedDate));
        selectedDate[sizeof(selectedDate)-1] = '\0';
        mode = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Daily Average")) { mode = 2; }
    ImGui::End();

    // --- Total Time Tracked Pane ---
    ImGui::Begin("Total Time Tracked");
    double totalSeconds = 0.0;
    if (mode == 0) {
        totalSeconds = getTotalTimeTrackedCurrentRun("");
        ImGui::Text("All-Time Tracked: %s", formatTime(totalSeconds).c_str());
    } else if (mode == 1) {
        std::string startDate(selectedDate);
        std::string endDate = getNextDate(startDate);
        totalSeconds = getTotalTimeTrackedCurrentRun(startDate, endDate);
        ImGui::Text("Time Tracked on %s: %s", selectedDate, formatTime(totalSeconds).c_str());
    } else if (mode == 2) {
        double daysTracked = getDaysTracked();
        totalSeconds = (daysTracked > 0) ? (getTotalTimeTrackedCurrentRun("") / daysTracked) : 0.0;
        ImGui::Text("Daily Average: %s", formatTime(totalSeconds).c_str());
    }
    ImGui::End();

    // --- Top 10 Applications Pane (Table) ---
    ImGui::Begin("Top 10 Applications");
    std::vector<ApplicationData> topApps;
    if (mode == 0) {
        topApps = getTopApplications("");
    } else if (mode == 1) {
        std::string startDate(selectedDate);
        std::string endDate = getNextDate(startDate);
        topApps = getTopApplications(startDate, endDate);
    } else if (mode == 2) {
        topApps = getTopApplications("");
        double daysTracked = getDaysTracked();
        for (auto &app : topApps) {
            if (daysTracked > 1)
                app.totalTime = app.totalTime / daysTracked;
        }
    }
    if (topApps.empty()) {
        float availWidth = ImGui::GetContentRegionAvail().x;
        float textWidth = ImGui::CalcTextSize("NO INFORMATION FOR THIS DATE").x;
        ImGui::SetCursorPosX((availWidth - textWidth) * 0.5f);
        ImGui::Text("NO INFORMATION FOR THIS DATE");
    }
    std::string hoveredTableProcess = "";
    if (ImGui::BeginTable("AppsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        if (mode == 2)
            ImGui::TableSetupColumn("Process"), ImGui::TableSetupColumn("Daily Average");
        else
            ImGui::TableSetupColumn("Process"), ImGui::TableSetupColumn("Total Time");
        ImGui::TableHeadersRow();
        for (const auto &app : topApps) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", app.processName.c_str());
            if (ImGui::IsItemHovered())
                hoveredTableProcess = app.processName;
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", formatTime(app.totalTime).c_str());
        }
        ImGui::EndTable();
    }
    ImGui::End();

    // --- Usage Pie Chart Pane ---
    ImGui::Begin("Usage Pie Chart");
    double overallTime = 0.0;
    if (mode == 0) {
        overallTime = getTotalTimeTrackedCurrentRun("");
    } else if (mode == 1) {
        std::string startDate(selectedDate);
        std::string endDate = getNextDate(startDate);
        overallTime = getTotalTimeTrackedCurrentRun(startDate, endDate);
    } else if (mode == 2) {
        double daysTracked = getDaysTracked();
        overallTime = (daysTracked > 1) ? (getTotalTimeTrackedCurrentRun("") / daysTracked) : getTotalTimeTrackedCurrentRun("");
    }
    if (overallTime <= 0.0) {
        float availWidth = ImGui::GetContentRegionAvail().x;
        float textWidth = ImGui::CalcTextSize("NO INFORMATION FOR THIS DATE").x;
        ImGui::SetCursorPosX((availWidth - textWidth) * 0.5f);
        ImGui::Text("NO INFORMATION FOR THIS DATE");
    } else {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 canvas_sz = ImVec2(300, 300);
        float offsetX = (avail.x - canvas_sz.x) * 0.5f;
        if (offsetX > 0)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("canvas", canvas_sz);
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
        ImVec2 center = ImVec2((canvas_p0.x + canvas_p1.x) * 0.5f, (canvas_p0.y + canvas_p1.y) * 0.5f);
        float radius = (canvas_sz.x < canvas_sz.y ? canvas_sz.x : canvas_sz.y) * 0.4f;
        std::string highlightProcess = hoveredTableProcess;
        std::vector<std::pair<PieSlice, std::pair<float, float>>> sliceAngles;
        DrawPieChart(topApps, overallTime, center, radius, highlightProcess, sliceAngles);
        if (highlightProcess.empty()) {
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            float dx = mousePos.x - center.x;
            float dy = mousePos.y - center.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist <= radius) {
                float mouseAngle = atan2f(dy, dx);
                if (mouseAngle < 0)
                    mouseAngle += 2 * IM_PI;
                for (const auto &entry : sliceAngles) {
                    float startAngle = entry.second.first;
                    float endAngle = entry.second.second;
                    if (mouseAngle >= startAngle && mouseAngle < endAngle) {
                        highlightProcess = entry.first.label;
                        break;
                    }
                }
            }
        }
        bool found = false;
        for (const auto &entry : sliceAngles) {
            if (entry.first.label == hoveredTableProcess) {
                found = true;
                break;
            }
        }
        if (!hoveredTableProcess.empty() && !found) {
            highlightProcess = "Other";
        }
        DrawPieChart(topApps, overallTime, center, radius, highlightProcess, sliceAngles);
        ImGui::Dummy(ImVec2(canvas_sz.x, 10));
        for (const auto &entry : sliceAngles) {
            if (entry.first.label == highlightProcess) {
                char labelBuffer[256];
                snprintf(labelBuffer, sizeof(labelBuffer), "Process: %s, Time: %s",
                         entry.first.label.c_str(), formatTime(entry.first.value).c_str());
                float textWidth = ImGui::CalcTextSize(labelBuffer).x;
                float availWidth = ImGui::GetContentRegionAvail().x;
                ImGui::SetCursorPosX((availWidth - textWidth) * 0.5f);
                ImGui::Text("%s", labelBuffer);
                break;
            }
        }
    }
    ImGui::End();

    // --- Calendar Pane ---
    ImGui::Begin("Calendar");
    DrawCalendar();
    ImGui::End();
}

//-----------------------------------------------------------------------------
// Main entry point of the application.
//-----------------------------------------------------------------------------
int main(int, char**)
{
    if (!initDatabase("activity_log.db")) {
        return 1;
    }
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L, 0L,
        GetModuleHandle(NULL),
        NULL,
        NULL,
        NULL,
        NULL,
        _T("ImGui Example"),
        NULL
    };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(
        wc.lpszClassName,
        _T("Application Tracker"),
        WS_OVERLAPPEDWINDOW,
        100, 100,
        1280, 800,
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );
    HDC hdc = ::GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);
    HGLRC hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);
    gl3wInit();

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");
    initializeCurrentDate();
    bool done = false;
    MSG msg;
    setDefaultTheme();
    std::string programStartTime = getCurrentJulianDay();
    printf(programStartTime.c_str());
    while (!done)
    {
        trackActiveWindowSession();
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        load_ImGui();
        checkActiveSessionIntegrity();
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hrc);
    ::ReleaseDC(hwnd, hdc);
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    endActiveSessions();
    return 0;
}
