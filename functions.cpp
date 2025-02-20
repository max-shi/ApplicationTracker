#include "functions.h"
#include "database.h"      // Provides getDatabase() and ensures the DB is initialized.
#include <sqlite3.h>
#include <iostream>
#include <ctime>
#include <string>
#include <cstdio>
#include <cmath>

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


std::string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    char buf[20]; // Format: "YYYY-MM-DD HH:MM:SS"
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return std::string(buf);
}

double getTotalTimeTrackedCurrentRun(const std::string &programStartTime) {
    sqlite3* dbHandle = getDatabase();
    if (!dbHandle) {
        std::cerr << "Database not initialized.\n";
        return 0.0;
    }

    // Query to sum session durations only for sessions with startTime >= programStartTime.
    const char* sql = R"(
        SELECT COALESCE(SUM(
            CASE
                WHEN endTime IS NOT NULL THEN (julianday(endTime) - julianday(startTime))
                ELSE (julianday('now') - julianday(startTime))
            END
        ), 0) as total_time
        FROM ActivitySession
        WHERE julianday(startTime) >= julianday(?);
    )";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(dbHandle, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare total time query: "
                  << sqlite3_errmsg(dbHandle) << std::endl;
        return 0.0;
    }

    // Bind the programStartTime to the SQL parameter.
    sqlite3_bind_text(stmt, 1, programStartTime.c_str(), -1, SQLITE_TRANSIENT);

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

