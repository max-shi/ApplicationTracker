cmake_minimum_required(VERSION 3.30)
project(tracker)

set(CMAKE_CXX_STANDARD 20)

# Set the directory where the SQLite files are located.
set(SQLITE3_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/sqlite")

# Include directories.
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/sqlite)
include_directories(${CMAKE_SOURCE_DIR}/imgui)
include_directories(${CMAKE_SOURCE_DIR}/imgui/backends)

# Find OpenGL (provided by your system on Windows).
find_package(OpenGL REQUIRED)

# List your source files for your executable.
set(SOURCES
        main.cpp
        tracker.cpp
        database.cpp
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_demo.cpp  # Optional: Remove if you don't want the demo code.
        imgui/backends/imgui_impl_win32.cpp
        imgui/backends/imgui_impl_opengl3.cpp
        GL/gl3w.c
)

# Build SQLite as a static library from the amalgamation source.
add_library(sqlite3 STATIC ${SQLITE3_INCLUDE_DIR}/sqlite3.c)

# Create the executable.
add_executable(tracker ${SOURCES})

# Link libraries: sqlite3 and OpenGL.
target_link_libraries(tracker sqlite3 ${OPENGL_LIBRARIES})

# On Windows, you might also need to link additional system libraries.
if(WIN32)
    target_link_libraries(tracker psapi)
endif()
