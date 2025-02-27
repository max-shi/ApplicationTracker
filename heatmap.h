//
// Created by Max on 26/02/2025.
//

#ifndef HEATMAP_H
#define HEATMAP_H
#include <imgui.h>
#include <string>

#include "functions.h"


struct HourlyUsageData {
    double totalUsage;                    // Total usage as fraction of hour (0.0-1.0)
    std::vector<ApplicationData> apps;    // Top apps used in this hour
};

ImVec4 getHeatMapColor(double percent);
void DrawHeatMap(const std::string& selectedDate);
std::array<double, 24> computeHourlyUsage(const std::string& selectedDate);
std::vector<ApplicationData> getTopApplicationsTimeRange(double queryStart, double queryEnd);
std::vector<ApplicationData> getHourlyApplicationData(const std::string& selectedDate, int hour);
std::array<HourlyUsageData, 24> computeDetailedHourlyUsage(const std::string& selectedDate);
ImU32 getAppColor(const std::string& appName);
void DrawAppCategoryPane();


#endif //HEATMAP_H
