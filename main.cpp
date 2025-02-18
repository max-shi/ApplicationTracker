#include <iostream>
#include <thread>
#include <chrono>
#include "database.h"
#include "tracker.h"

int main() {
    // Initialize the SQLite database (creates file "activity_log.db").
    if (!initDatabase("activity_log.db")) {
        return 1;
    }

    // Main loop: check for active window changes every second.
    while (true) {
        trackActiveWindowSession();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    closeDatabase();
    return 0;
}
