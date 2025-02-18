#include <iostream>
#include <thread>
#include <chrono>
#include "database.h"
#include "tracker.h"

int main() {
    // Initialize SQLite database (creates file "activity_log.db")
    if (!initDatabase("activity_log.db")) {
        return 1;
    }

    // Main loop: poll the active window periodically (every second)
    while (true) {
        trackActiveWindow();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Note: This line is not reached because of the infinite loop.
    closeDatabase();
    return 0;
}
