#include "database.h"
#include <sqlite3.h>
#include <iostream>
#include <string>

// Global pointer for SQLite database
static sqlite3* db = nullptr;


// function which initialises the database connection
// param --> database path
// returns a boolean wether success of failure
bool initDatabase(const std::string& dbPath) {
    // attempt to open the SQLite connection (using the database path)
    // rc (return code) is if the connection is valid (valid if RC = SQLITE_OK)
    int rc = sqlite3_open(dbPath.c_str(), &db);

    // if the return code for the connection is valid ..
    if (rc != SQLITE_OK) {
        // std::cerr is the standard error stream
        // so if rc != sqlite_ok, then we add an error message.
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        // also return false
        return false;
    }

    // SQL statement to create the table if it doesn't exist
    std::string sql = R"(
        CREATE TABLE IF NOT EXISTS ActivityLog (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            processName TEXT,
            windowTitle TEXT
        );
    )";

    // errMsg will store any error message
    char* errMsg = nullptr;
    // execute the add table command
    // check for error code stuff, add to error stream.
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    return true;
 }

// Function to log new activity
bool logActivity(const std::string& processName, const std::string& windowTitle) {
    std::string sql = "INSERT INTO ActivityLog (processName, windowTitle) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    // Bind the parameters to the SQL statement
    sqlite3_bind_text(stmt, 1, processName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, windowTitle.c_str(), -1, SQLITE_TRANSIENT);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
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
