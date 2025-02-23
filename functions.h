#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>
#include <vector>

// Structure to hold the details of a tracked application.
struct ApplicationData {
    std::string processName;
    std::string windowTitle;
    std::string startTime;  // The timestamp when tracking began.
    double totalTime = 0.0; // Total usage time in seconds (for aggregated data)
};

// Returns true if an active (current) session is found.
// Fills appData with process name, window title, and start time.
bool getCurrentTrackedApplication(ApplicationData &appData);
std::vector<ApplicationData> getTopApplications(const std::string &startDate, const std::string &endDate = "");

// Obtain the current timestamp, (used when the program is first launched)
std::string getCurrentTimestamp();


std::string julianToCalendarString(double JD);
void endActiveSessions();

std::string getCurrentJulianDay();
std::string getNextDate(const std::string &date);
// get total time in seconds from original timestamp
double getTotalTimeTrackedCurrentRun(const std::string &startDate, const std::string &endDate = "");
#endif // FUNCTIONS_H
