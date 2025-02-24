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
        // When the window is resized, update the OpenGL viewport if not minimized.
        if (wParam != SIZE_MINIMIZED)
        {
            int width = LOWORD(lParam);  // Get the new width.
            int height = HIWORD(lParam);  // Get the new height.
            glViewport(0, 0, width, height); // Set the OpenGL viewport to cover the new window size.
        }
        return 0;
    case WM_SYSCOMMAND:
        // Disable the ALT application menu.
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        // When the window is destroyed, post a quit message to exit the application.
        PostQuitMessage(0);
        return 0;
    }
    // For all other messages, use the default window procedure.
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
void initializeCurrentDate() {
    std::time_t t = std::time(nullptr);
    std::tm tm;
    // For MSVC use localtime_s, for POSIX use localtime_r or simply localtime.
    localtime_s(&tm, &t);
    std::strftime(selectedDate, sizeof(selectedDate), "%Y-%m-%d", &tm);
}






void load_ImGui() {
    // --- Controls Pane ---
    ImGui::Begin("Controls");

    // Date input field.
    ImGui::InputText("Date (YYYY-MM-DD)", selectedDate, sizeof(selectedDate));
    //ImGui::SameLine();
    // Button to go to the previous day.

    // Mode selection buttons.
    if (ImGui::Button("All-Time")) {
        mode = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Day Total")) {
        mode = 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("<")) {
        std::string newDate = getPreviousDate(std::string(selectedDate));
        strncpy(selectedDate, newDate.c_str(), sizeof(selectedDate));
        selectedDate[sizeof(selectedDate) - 1] = '\0';
    }
    ImGui::SameLine();
    // Button to go to the next day.
    if (ImGui::Button(">")) {
        std::string newDate = getNextDate(std::string(selectedDate));
        strncpy(selectedDate, newDate.c_str(), sizeof(selectedDate));
        selectedDate[sizeof(selectedDate) - 1] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("Daily Average")) {
        mode = 2;
    }
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
        if (daysTracked > 0)
            totalSeconds = getTotalTimeTrackedCurrentRun("") / daysTracked;
        else
            totalSeconds = 0.0;
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
        // Center the "NO INFORMATION FOR THIS DATE" message.
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

    // Reserve a square region for the pie chart.
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImVec2(300, 300); // Adjust as needed.
    ImGui::InvisibleButton("canvas", canvas_sz);
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
    ImVec2 center = ImVec2((canvas_p0.x + canvas_p1.x) * 0.5f, (canvas_p0.y + canvas_p1.y) * 0.5f);
    float radius = (canvas_sz.x < canvas_sz.y ? canvas_sz.x : canvas_sz.y) * 0.4f;

    // Determine overall tracked time.
    double overallTime = 0.0;
    if (mode == 0) {
        overallTime = getTotalTimeTrackedCurrentRun("");
    } else if (mode == 1) {
        std::string startDate(selectedDate);
        std::string endDate = getNextDate(startDate);
        overallTime = getTotalTimeTrackedCurrentRun(startDate, endDate);
    } else if (mode == 2) {
        double daysTracked = getDaysTracked();
        if (daysTracked > 1) {
            overallTime = getTotalTimeTrackedCurrentRun("") / daysTracked;
        } else {
            overallTime = getTotalTimeTrackedCurrentRun("");
        }
    }
    if (overallTime <= 0.0) {
        // Center the "NO INFORMATION FOR THIS DATE" message.
        float availWidth = ImGui::GetContentRegionAvail().x;
        float textWidth = ImGui::CalcTextSize("NO INFORMATION FOR THIS DATE").x;
        ImGui::SetCursorPosX((availWidth - textWidth) * 0.5f);
        ImGui::Text("NO INFORMATION FOR THIS DATE");
    }
    // Determine the process to highlight.
    // Priority: use the hovered table process, if available.
    std::string highlightProcess = hoveredTableProcess;

    // Vector to capture each slice’s angle boundaries.
    std::vector<std::pair<PieSlice, std::pair<float, float>>> sliceAngles;
    DrawPieChart(topApps, overallTime, center, radius, highlightProcess, sliceAngles);

    // If no table row is hovered, use mouse hover over the pie chart to set the highlight.
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

    // If a table process was hovered but not found among the drawn slices,
    // it belongs to the aggregated "Other" group.
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

    // Redraw the pie chart with the updated highlight.
    DrawPieChart(topApps, overallTime, center, radius, highlightProcess, sliceAngles);

    // Display label below the pie chart for the highlighted slice.
    ImGui::Dummy(ImVec2(canvas_sz.x, 10)); // Spacer
    for (const auto &entry : sliceAngles) {
        if (entry.first.label == highlightProcess) {
            ImGui::Text("Process: %s, Time: %s", entry.first.label.c_str(), formatTime(entry.first.value).c_str());
            break;
        }
    }
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
    // --- Create application window ---
    // Define and initialize a window class structure.
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),            // Size of the structure.
        CS_CLASSDC,                    // Class style.
        WndProc,                       // Pointer to the window procedure.
        0L, 0L,                        // Extra memory for class and window.
        GetModuleHandle(NULL),         // Handle to the application instance.
        NULL,                          // No icon.
        NULL,                          // No cursor.
        NULL,                          // No background brush.
        NULL,                          // No menu.
        _T("ImGui Example"),           // Window class name.
        NULL                           // No small icon.
    };
    // Register the window class with Windows.
    ::RegisterClassEx(&wc);
    // Create the window using the registered class.
    HWND hwnd = ::CreateWindow(
        wc.lpszClassName,              // The class name to use.
        _T("Application Tracker"), // Window title.
        WS_OVERLAPPEDWINDOW,           // Window style.
        100, 100,                      // Initial x and y position.
        1280, 800,                     // Width and height of the window.
        NULL,                          // No parent window.
        NULL,                          // No menu.
        wc.hInstance,                  // Application instance handle.
        NULL                           // No additional parameters.
    );

    // --- Initialize OpenGL context ---
    // Retrieve the device context (DC) for the window.
    HDC hdc = ::GetDC(hwnd);
    // Define the pixel format descriptor (PFD) for our OpenGL context.
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),   // Size of this descriptor.
        1,                               // Version number.
        PFD_DRAW_TO_WINDOW |             // The format must support drawing to a window.
        PFD_SUPPORT_OPENGL |             // The format must support OpenGL.
        PFD_DOUBLEBUFFER,                // The format must support double buffering.
        PFD_TYPE_RGBA,                   // Request an RGBA format.
        32,                              // 32-bit color depth.
        0, 0, 0, 0, 0, 0,                // Color bits (ignored here).
        0,                               // No alpha buffer.
        0,                               // Alpha shift (ignored).
        0,                               // No accumulation buffer.
        0, 0, 0, 0,                      // Accumulation bits (ignored).
        24,                              // 24-bit depth buffer (Z-buffer).
        8,                               // 8-bit stencil buffer.
        0,                               // No auxiliary buffer.
        PFD_MAIN_PLANE,                  // Main drawing layer.
        0,                               // Reserved.
        0, 0, 0                          // Layer masks (ignored).
    };
    // Choose a pixel format that matches the given descriptor.
    int pf = ChoosePixelFormat(hdc, &pfd);
    // Set the chosen pixel format for the device context.
    SetPixelFormat(hdc, pf, &pfd);
    // Create an OpenGL rendering context.
    HGLRC hrc = wglCreateContext(hdc);
    // Make the created OpenGL context current.
    wglMakeCurrent(hdc, hrc);
    // Initialize OpenGL functions using gl3w.
    gl3wInit();

    // Show the window on the screen.
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    // Update the window to ensure it is painted.
    ::UpdateWindow(hwnd);

    // --- Setup Dear ImGui context ---
    IMGUI_CHECKVERSION();           // Ensure the ImGui version is correct.
    ImGui::CreateContext();         // Create a new ImGui context.
    ImGuiIO& io = ImGui::GetIO();   // Get a reference to the IO structure (used for configuration).
    (void)io;                      // Avoid unused variable warning.
    ImGui::StyleColorsDark();       // Set the ImGui style to a dark theme.

    // Initialize the ImGui platform/renderer bindings for Win32 and OpenGL3.
    ImGui_ImplWin32_Init(hwnd);     // Initialize the Win32 backend with our window.
    ImGui_ImplOpenGL3_Init("#version 130");  // Initialize the OpenGL3 backend with the GLSL version string.
    initializeCurrentDate();
    // --- Main loop ---
    bool done = false;  // Main loop flag.
    MSG msg;            // Structure for Windows messages.
    setDefaultTheme();
    // start timestamp for the overall session
    std::string programStartTime = getCurrentJulianDay();
    printf(programStartTime.c_str());
    while (!done)
    {
        // Process all pending Windows messages.
        trackActiveWindowSession();
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);  // Translate virtual-key messages.
            ::DispatchMessage(&msg);   // Dispatch the message to our window procedure.
            if (msg.message == WM_QUIT)
                done = true;           // Exit loop if a quit message is received.
        }
        if (done)
            break;
        // Start a new frame for ImGui (prepare for UI rendering).
        ImGui_ImplOpenGL3_NewFrame(); // Start new frame for OpenGL3.
        ImGui_ImplWin32_NewFrame();   // Start new frame for Win32.
        ImGui::NewFrame();
        load_ImGui();
        // error checking
        checkActiveSessionIntegrity();
        // --- Rendering ---
        // Finalize the ImGui frame and prepare the draw data.
        ImGui::Render();
        // Set the OpenGL viewport to cover the entire display.
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        // Set a background color.
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        // Clear the color buffer.
        glClear(GL_COLOR_BUFFER_BIT);
        // Render the ImGui draw data using the OpenGL3 backend.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // Swap the front and back buffers to display the rendered frame.
        SwapBuffers(hdc);
    }

    // --- Cleanup ---
    // Shutdown and cleanup the ImGui OpenGL3 backend.
    ImGui_ImplOpenGL3_Shutdown();
    // Shutdown and cleanup the ImGui Win32 backend.
    ImGui_ImplWin32_Shutdown();
    // Destroy the ImGui context.
    ImGui::DestroyContext();

    // Release and clean up the OpenGL context and device context.
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hrc);
    ::ReleaseDC(hwnd, hdc);
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
    endActiveSessions();
    return 0; // Exit the application.
}



