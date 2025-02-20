#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>

// Structure to hold the details of a tracked application.
struct ApplicationData {
    std::string processName;
    std::string windowTitle;
    std::string startTime;  // The timestamp when tracking began.
};

// Returns true if an active (current) session is found.
// Fills appData with process name, window title, and start time.
bool getCurrentTrackedApplication(ApplicationData &appData);


// Obtain the current timestamp, (used when the program is first launched)
std::string getCurrentTimestamp();

std::string getCurrentJulianDay();

// get total time in seconds from original timestamp
double getTotalTimeTrackedCurrentRun(const std::string &programStartTime);
#endif // FUNCTIONS_H
