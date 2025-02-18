cmake_minimum_required(VERSION 3.30)
project(tracker)

set(CMAKE_CXX_STANDARD 20)

# Set the directory where the SQLite files are located
set(SQLITE3_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/sqlite")
include_directories(${SQLITE3_INCLUDE_DIR})

# Build SQLite as a static library from the amalgamation source
add_library(sqlite3 STATIC ${SQLITE3_INCLUDE_DIR}/sqlite3.c)


add_executable(tracker main.cpp
        database.cpp
        database.h
        tracker.cpp
        tracker.h
)

# Link libraries. psapi is a system library on Windows.
target_link_libraries(tracker sqlite3 psapi)