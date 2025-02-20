// Include core ImGui functionality.
// including backends and gl3w (API functions et c)
#include <chrono>
#include <database.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include <GL/gl3w.h>
#include <windows.h>
#include <tchar.h>
#include <thread>
#include <tracker.h>
#include "functions.h"


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
        _T("Dear ImGui - Basic Pane"), // Window title.
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

    // --- Main loop ---
    bool done = false;  // Main loop flag.
    MSG msg;            // Structure for Windows messages.

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
        ImGui::NewFrame();            // Start a new ImGui frame.

        // --- New Pane: Current Tracked Application ---
        // Call our function to retrieve the current active session from the database.
        ApplicationData currentApp;
        bool hasCurrentApp = getCurrentTrackedApplication(currentApp);
        ImGui::Begin("Current Tracked Application");  // Start a new pane.
        if (hasCurrentApp) {
            // Display process name, window title, and start time.
            ImGui::Text("Process: %s", currentApp.processName.c_str());
            ImGui::Text("Window: %s", currentApp.windowTitle.c_str());
            ImGui::Text("Started at: %s", julianToCalendarString(std::stod(programStartTime)).c_str());
        } else {
            ImGui::Text("No active session found.");
        }
        ImGui::End();

        // --- Total Time Tracked Pane ---
        ImGui::Begin("Total Time Tracked");

        // Get the total time tracked (in seconds) since programStartTime.
        double totalSeconds = getTotalTimeTrackedCurrentRun(programStartTime);

        // Convert to hours, minutes, seconds (if desired):
        int hours = static_cast<int>(totalSeconds) / 3600;
        int minutes = (static_cast<int>(totalSeconds) % 3600) / 60;
        int seconds = static_cast<int>(totalSeconds) % 60;

        // Display the total time in a friendly format.
        ImGui::Text("Total time tracked: %d hours, %d minutes, %d seconds", hours, minutes, seconds);

        ImGui::End();

        ImGui::Begin("Top 10 Applications");
        {
            // Retrieve the top applications since the program started.
            std::vector<ApplicationData> topApps = getTopApplications(programStartTime);

            if (ImGui::BeginTable("AppsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                // Define columns.
                ImGui::TableSetupColumn("Process");
                ImGui::TableSetupColumn("Total Time (seconds)");
                ImGui::TableHeadersRow();

                // Populate table rows.
                for (const auto &app : topApps) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", app.processName.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", app.totalTime);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

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

    return 0; // Exit the application.
}
