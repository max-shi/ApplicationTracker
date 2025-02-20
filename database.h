#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include <string>

// Initializes the SQLite database and creates the ActivitySession table.
bool initDatabase(const std::string& dbPath);

// Starts a new session and returns the session id via 'sessionId'.
bool startSession(const std::string& processName, const std::string& windowTitle, int & sessionId);

// Ends an existing session by updating its end time.
bool endSession(int sessionId);

// Closes the database connection.
void closeDatabase();

sqlite3* getDatabase();

#endif
