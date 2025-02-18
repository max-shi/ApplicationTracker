#ifndef DATABASE_H
#define DATABASE_H

#include <string>

// Initializes the SQLite database (creates file and table if needed)
bool initDatabase(const std::string& dbPath);

// Logs an activity record into the database
bool logActivity(const std::string& processName, const std::string& windowTitle);

// Closes the SQLite database connection
void closeDatabase();

#endif
