#include "functions.h"
#include "database.h"      // Provides getDatabase() and ensures the DB is initialized.
#include <sqlite3.h>
#include <iostream>

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
