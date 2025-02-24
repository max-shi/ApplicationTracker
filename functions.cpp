#include "functions.h"
#include "database.h"      // Provides getDatabase() and ensures the DB is initialized.
#include <sqlite3.h>
#include <iostream>
#include <ctime>
#include <string>
#include <cstdio>
#include <cmath>
#include <imgui.h>
#include <iomanip>
#include <vector>

// Implementation of getCurrentTrackedApplication:
// It queries the ActivitySession table for the active session (where endTime is NULL).
bool getCurrentTrackedApplication(ApplicationData &appData) {
    // Get the database handle from the database module.
    sqlite3* dbHandle = getDatabase();
    if (!dbHandle) {
        std::cerr << "Database not initialized.\n";
        return false;
    }
    // SQL query to retrieve the current active session.
    const char* sql = "SELECT processName, windowTitle, startTime FROM ActivitySession WHERE endTime IS NULL ORDER BY id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    // Prepare the SQL statement.
    int rc = sqlite3_prepare_v2(dbHandle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(dbHandle) << std::endl;
        return false;
    }
    // Execute the query and check for a returned row.
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // Retrieve columns: processName, windowTitle, and startTime.
        const unsigned char* procName = sqlite3_column_text(stmt, 0);
        const unsigned char* winTitle = sqlite3_column_text(stmt, 1);
        const unsigned char* startTime = sqlite3_column_text(stmt, 2);
        appData.processName = procName ? reinterpret_cast<const char*>(procName) : "";
        appData.windowTitle = winTitle ? reinterpret_cast<const char*>(winTitle) : "";
        appData.startTime = startTime ? reinterpret_cast<const char*>(startTime) : "";
        sqlite3_finalize(stmt);
        return true;
    } else {
        // No active session found.
        sqlite3_finalize(stmt);
        return false;
    }
}

std::string julianToCalendarString(double JD) {
    // Adjust JD so that the day starts at midnight.
    double J = JD + 0.5;
    int Z = static_cast<int>(std::floor(J));
    double F = J - Z;

    int A;
    if (Z < 2299161) {
        A = Z;
    } else {
        int alpha = static_cast<int>(std::floor((Z - 1867216.25) / 36524.25));
        A = Z + 1 + alpha - static_cast<int>(std::floor(alpha / 4.0));
    }

    int B = A + 1524;
    int C = static_cast<int>(std::floor((B - 122.1) / 365.25));
    int D = static_cast<int>(std::floor(365.25 * C));
    int E = static_cast<int>(std::floor((B - D) / 30.6001));

    double dayDecimal = B - D - std::floor(30.6001 * E) + F;
    int day = static_cast<int>(dayDecimal);
    double dayFraction = dayDecimal - day;

    int month;
    if (E < 14)
        month = E - 1;
    else
        month = E - 13;

    int year;
    if (month > 2)
        year = C - 4716;
    else
        year = C - 4715;

    // Convert the fractional part of the day into hours, minutes, and seconds.
    double totalSeconds = dayFraction * 86400.0;  // 86400 seconds in a day
    int hour = static_cast<int>(totalSeconds / 3600);
    totalSeconds -= hour * 3600;
    int minute = static_cast<int>(totalSeconds / 60);
    totalSeconds -= minute * 60;
    int second = static_cast<int>(totalSeconds + 0.5);  // Round to nearest second

    // Format into "YYYY-MM-DD HH:MM:SS"
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  year, month, day, hour, minute, second);
    // printf(std::string(buf).c_str());
    return std::string(buf);
}


std::string getCurrentJulianDay() {
    // Get current time in local time.
    std::time_t now = std::time(nullptr);
    std::tm *lt = std::localtime(&now);
    int year = lt->tm_year + 1900;
    int month = lt->tm_mon + 1;
    int day = lt->tm_mday;
    int hour = lt->tm_hour;
    int minute = lt->tm_min;
    int second = lt->tm_sec;

    // Adjust months so that January and February are counted as months 13 and 14 of the previous year.
    if (month <= 2) {
        year -= 1;
        month += 12;
    }

    // Calculate the terms needed for the Julian day formula.
    int A = year / 100;
    int B = 2 - A + (A / 4);

    // Compute the fractional day.
    double dayFraction = (hour + (minute / 60.0) + (second / 3600.0)) / 24.0;

    // Compute the Julian day number.
    double JD = std::floor(365.25 * (year + 4716))
              + std::floor(30.6001 * (month + 1))
              + day + dayFraction + B - 1524.5;

    // Format the result into a string.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.10f", JD);
    return std::string(buf);
}


// std::string getCurrentTimestamp() {
//     std::time_t now = std::time(nullptr);
//     char buf[20]; // Format: "YYYY-MM-DD HH:MM:SS"
//     std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
//     return std::string(buf);
// }

// Retrieve the top 10 applications (by processName) since programStartTime.
std::vector<ApplicationData> getTopApplications(const std::string &startDate, const std::string &endDate) {
    std::vector<ApplicationData> results;
    sqlite3* dbHandle = getDatabase();
    if (!dbHandle) {
        std::cerr << "Database not initialized.\n";
        return results;
    }

    // SQL for ALL-TIME (no date filter)
    const char* sqlAllTime = R"(
        SELECT processName, COALESCE(SUM(
            CASE
                WHEN endTime IS NOT NULL THEN (julianday(endTime) - julianday(startTime))
                ELSE (julianday('now','localtime') - julianday(startTime))
            END
        ), 0) as total_time
        FROM ActivitySession
        GROUP BY processName
        ORDER BY total_time DESC
        LIMIT 10;
    )";

    // SQL for a specific date range
    const char* sqlDateRange = R"(
        SELECT processName, COALESCE(SUM(
            CASE
                WHEN endTime IS NOT NULL THEN (julianday(endTime) - julianday(startTime))
                ELSE (julianday('now','localtime') - julianday(startTime))
            END
        ), 0) as total_time
        FROM ActivitySession
        WHERE julianday(startTime) >= julianday(?)
          AND julianday(startTime) < julianday(?)
        GROUP BY processName
        ORDER BY total_time DESC
        LIMIT 10;
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc;
    if (endDate.empty()) {
        // Use all-time query.
        rc = sqlite3_prepare_v2(dbHandle, sqlAllTime, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare top applications query: "
                      << sqlite3_errmsg(dbHandle) << std::endl;
            return results;
        }
    } else {
        // Use date-range query. Bind both startDate and endDate.
        rc = sqlite3_prepare_v2(dbHandle, sqlDateRange, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare top applications date range query: "
                      << sqlite3_errmsg(dbHandle) << std::endl;
            return results;
        }
        sqlite3_bind_text(stmt, 1, startDate.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, endDate.c_str(), -1, SQLITE_TRANSIENT);
    }

    // Process each row in the result.
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ApplicationData app;
        const unsigned char* procName = sqlite3_column_text(stmt, 0);
        double totalDays = sqlite3_column_double(stmt, 1);
        app.processName = procName ? reinterpret_cast<const char*>(procName) : "";
        app.totalTime = totalDays * 86400.0; // convert days to seconds
        results.push_back(app);
    }

    sqlite3_finalize(stmt);
    return results;
}



double getTotalTimeTrackedCurrentRun(const std::string &startDate, const std::string &endDate) {
    sqlite3* dbHandle = getDatabase();
    if (!dbHandle) {
        std::cerr << "Database not initialized.\n";
        return 0.0;
    }

    // SQL for ALL-TIME
    const char* sqlAllTime = R"(
        SELECT COALESCE(SUM(
            CASE
                WHEN endTime IS NOT NULL THEN (julianday(endTime) - julianday(startTime))
                ELSE (julianday('now','localtime') - julianday(startTime))
            END
        ), 0) as total_time
        FROM ActivitySession;
    )";

    // SQL for a specific date range
    const char* sqlDateRange = R"(
        SELECT COALESCE(SUM(
            CASE
                WHEN endTime IS NOT NULL THEN (julianday(endTime) - julianday(startTime))
                ELSE (julianday('now','localtime') - julianday(startTime))
            END
        ), 0) as total_time
        FROM ActivitySession
        WHERE julianday(startTime) >= julianday(?)
          AND julianday(startTime) < julianday(?);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc;
    if (endDate.empty()) {
        rc = sqlite3_prepare_v2(dbHandle, sqlAllTime, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare total time query: "
                      << sqlite3_errmsg(dbHandle) << std::endl;
            return 0.0;
        }
    } else {
        rc = sqlite3_prepare_v2(dbHandle, sqlDateRange, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare total time date range query: "
                      << sqlite3_errmsg(dbHandle) << std::endl;
            return 0.0;
        }
        sqlite3_bind_text(stmt, 1, startDate.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, endDate.c_str(), -1, SQLITE_TRANSIENT);
    }

    double totalDays = 0.0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        totalDays = sqlite3_column_double(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        std::cerr << "Failed to retrieve total time: "
                  << sqlite3_errmsg(dbHandle) << std::endl;
    }
    sqlite3_finalize(stmt);
    return totalDays * 86400.0; // Convert days to seconds.
}

std::string getNextDate(const std::string &date) {
    std::tm tm = {};
    std::istringstream ss(date);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        // If parsing fails, return the original string.
        return date;
    }
    tm.tm_mday += 1;
    // Normalize the tm structure.
    mktime(&tm);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}

std::string getPreviousDate(const std::string &date) {
    std::tm tm = {};
    std::istringstream ss(date);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        return date; // fallback if parsing fails
    }
    tm.tm_mday -= 1;
    // Normalize the tm structure.
    mktime(&tm);
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return std::string(buf);
}


void endActiveSessions() {
    sqlite3* dbHandle = getDatabase();
    if (!dbHandle) {
        std::cerr << "Database not initialized.\n";
        return;
    }
    const char* sql = "UPDATE ActivitySession SET endTime = julianday('now','localtime') WHERE endTime IS NULL;";
    char* errMsg = nullptr;
    int rc = sqlite3_exec(dbHandle, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "Error ending active sessions: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

// Compute the number of days tracked by the application.
double getDaysTracked() {
    sqlite3* dbHandle = getDatabase();
    if (!dbHandle) {
        std::cerr << "Database not initialized.\n";
        return 0.0;
    }
    const char* sql = R"(
        SELECT MIN(julianday(startTime)), MAX(COALESCE(endTime, julianday('now','localtime')))
        FROM ActivitySession;
    )";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(dbHandle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare getDaysTracked query: "
                  << sqlite3_errmsg(dbHandle) << std::endl;
        return 0.0;
    }
    double firstDay = 0.0, lastDay = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        firstDay = sqlite3_column_double(stmt, 0);
        lastDay = sqlite3_column_double(stmt, 1);
    }
    sqlite3_finalize(stmt);
    return lastDay - firstDay; // Difference in days (may be fractional).
}

// Format seconds into "H:MM:SS"
std::string formatTime(double totalSeconds) {
    int hours = static_cast<int>(totalSeconds) / 3600;
    int minutes = (static_cast<int>(totalSeconds) % 3600) / 60;
    int seconds = static_cast<int>(totalSeconds) % 60;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hours, minutes, seconds);
    return std::string(buffer);
}

void checkActiveSessionIntegrity() {
    sqlite3* dbHandle = getDatabase();
    if (!dbHandle) {
        std::cerr << "Database not initialized.\n";
        return;
    }

    // Query sessions with a NULL endTime ordered by startTime (earliest first).
    const char* sql = "SELECT id FROM ActivitySession WHERE endTime IS NULL ORDER BY startTime ASC;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(dbHandle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare active session query: "
                  << sqlite3_errmsg(dbHandle) << std::endl;
        return;
    }

    int count = 0;
    int earliestSessionId = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        count++;
        if (count == 1) {
            // The first row is the earliest session.
            earliestSessionId = id;
        }
    }
    sqlite3_finalize(stmt);

    if (count > 1) {
        std::cerr << "Error: More than one active session detected (endTime is NULL): "
                  << count << std::endl;
        // Update the earliest session with current time as the endTime.
        const char* updateSql = "UPDATE ActivitySession SET endTime = julianday('now','localtime') WHERE id = ?;";
        sqlite3_stmt* updateStmt = nullptr;
        rc = sqlite3_prepare_v2(dbHandle, updateSql, -1, &updateStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare update statement: "
                      << sqlite3_errmsg(dbHandle) << std::endl;
            return;
        }
        sqlite3_bind_int(updateStmt, 1, earliestSessionId);
        rc = sqlite3_step(updateStmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to update session with id " << earliestSessionId
                      << ": " << sqlite3_errmsg(dbHandle) << std::endl;
        } else {
            std::cout << "Fixed error: Session with id " << earliestSessionId
                      << " has been closed (endTime set to now)." << std::endl;
        }
        sqlite3_finalize(updateStmt);
    }
}


// GUI nonsense
void setDefaultTheme() {
    // Colors
    ImVec4 blackSemi = ImVec4(0.00f, 0.00f, 0.00f, 0.94f);
    ImVec4 black = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    ImVec4 red = ImVec4(0.23f, 0.16f, 0.16f, 1.00f);
    ImVec4 beige = ImVec4(0.78f, 0.62f, 0.51f, 1.00f);
    ImVec4 darkGrey = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    ImVec4 greenGrey = ImVec4(0.148f, 0.168f, 0.153f, 1.00f);
    ImVec4 white = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    ImVec4 darkerGrey = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
    ImVec4 grey = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    ImVec4 green = ImVec4(0.21f, 0.27f, 0.27f, 1.00f);

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = beige;
    colors[ImGuiCol_TextDisabled]           = white;
    colors[ImGuiCol_WindowBg]               = black;
    colors[ImGuiCol_ChildBg]                = black;
    colors[ImGuiCol_PopupBg]                = black;
    colors[ImGuiCol_Border]                 = beige;
    colors[ImGuiCol_BorderShadow]           = black;
    colors[ImGuiCol_FrameBg]                = red;
    colors[ImGuiCol_FrameBgHovered]         = darkerGrey;
    colors[ImGuiCol_FrameBgActive]          = black;
    colors[ImGuiCol_TitleBg]                = black;
    colors[ImGuiCol_TitleBgActive]          = black;
    colors[ImGuiCol_TitleBgCollapsed]       = black;
    colors[ImGuiCol_MenuBarBg]              = red;
    colors[ImGuiCol_ScrollbarBg]            = black;
    colors[ImGuiCol_ScrollbarGrab]          = red;
    colors[ImGuiCol_ScrollbarGrabHovered]   = darkerGrey;
    colors[ImGuiCol_ScrollbarGrabActive]    = grey;
    colors[ImGuiCol_CheckMark]              = beige;
    colors[ImGuiCol_SliderGrab]             = beige;
    colors[ImGuiCol_SliderGrabActive]       = beige;
    colors[ImGuiCol_Button]                 = red;
    colors[ImGuiCol_ButtonHovered]          = darkerGrey;
    colors[ImGuiCol_ButtonActive]           = red;
    colors[ImGuiCol_Header]                 = red;
    colors[ImGuiCol_HeaderHovered]          = darkerGrey;
    colors[ImGuiCol_HeaderActive]           = black;
    colors[ImGuiCol_Separator]              = beige;
    colors[ImGuiCol_SeparatorHovered]       = darkerGrey;
    colors[ImGuiCol_SeparatorActive]        = beige;
    colors[ImGuiCol_ResizeGrip]             = black;
    colors[ImGuiCol_ResizeGripHovered]      = darkerGrey;
    colors[ImGuiCol_ResizeGripActive]       = black;
    colors[ImGuiCol_Tab]                    = red;
    colors[ImGuiCol_TabHovered]             = darkerGrey;
    colors[ImGuiCol_TabActive]              = darkerGrey;
    colors[ImGuiCol_TabUnfocused]           = black;
    colors[ImGuiCol_TabUnfocusedActive]     = black;
    colors[ImGuiCol_PlotLines]              = beige;
    colors[ImGuiCol_PlotLinesHovered]       = darkerGrey;
    colors[ImGuiCol_PlotHistogram]          = beige;
    colors[ImGuiCol_PlotHistogramHovered]   = darkerGrey;
    colors[ImGuiCol_TextSelectedBg]         = black;
    colors[ImGuiCol_DragDropTarget]         = beige;
    colors[ImGuiCol_NavHighlight]           = black;
    colors[ImGuiCol_NavWindowingHighlight]  = black;
    colors[ImGuiCol_NavWindowingDimBg]      = black;
    colors[ImGuiCol_ModalWindowDimBg]       = black;

    // IO
    ImGuiIO& io = ImGui::GetIO();
    // io.FontGlobalScale = 1.6f;
}