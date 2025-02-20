#include "database.h"
#include <sqlite3.h>
#include <iostream>
#include <string>

// Global pointer for SQLite database.
static sqlite3* db = nullptr;


sqlite3* getDatabase() {
    return db;
}


bool initDatabase(const std::string& dbPath) {
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Create the ActivitySession table if it doesn't exist.
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS ActivitySession (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            processName TEXT,
            windowTitle TEXT,
            startTime DATETIME DEFAULT CURRENT_TIMESTAMP,
            endTime DATETIME
        );
    )";

    char* errMsg = nullptr;
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool startSession(const std::string& processName, const std::string& windowTitle, int & sessionId) {
    std::string sql = "INSERT INTO ActivitySession (processName, windowTitle) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare startSession statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Bind parameters.
    sqlite3_bind_text(stmt, 1, processName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, windowTitle.c_str(), -1, SQLITE_TRANSIENT);

    // Execute the statement.
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute startSession statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    // Retrieve the last inserted row id as the session id.
    sessionId = static_cast<int>(sqlite3_last_insert_rowid(db));
    return true;
}

bool endSession(int sessionId) {
    // Only update if sessionId is valid.
    if (sessionId <= 0) return false;

    std::string sql = "UPDATE ActivitySession SET endTime = datetime('now') WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare endSession statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_int(stmt, 1, sessionId);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute endSession statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

void closeDatabase() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}
