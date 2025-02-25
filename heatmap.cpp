//
// Created by Max on 26/02/2025.
//

#include "heatmap.h"

#include <array>

#include "functions.h"
#include "database.h"      // Provides getDatabase() and ensures the DB is initialized.
#include <sqlite3.h>
#include <iostream>
#include <ctime>
#include <string>
#include <cstdio>
#include <cmath>
#include <imgui.h>
#include <iomanip>
#include <vector>


std::array<double, 24> computeHourlyUsage(const std::string& selectedDate)
{
    std::array<double, 24> usage{};
    usage.fill(0.0);

    // Determine the start and end (Julian day) of the selected date.
    double dayStart = getJulianDayFromDate(selectedDate);
    std::string nextDate = getNextDate(selectedDate); // Assumes getNextDate is defined.
    double dayEnd = getJulianDayFromDate(nextDate);

    // SQL: Get sessions overlapping the selected day.
    // Conditions:
    //   session.startTime < dayEnd
    //   AND (session.endTime > dayStart OR session.endTime IS NULL)
    std::string sql =
        "SELECT startTime, endTime FROM ActivitySession "
        "WHERE startTime < ? AND (endTime > ? OR endTime IS NULL);";

    sqlite3* db = getDatabase();
    if (!db) {
        std::cerr << "Database not initialized." << std::endl;
        return usage;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
        return usage;
    }
    // Bind parameters: first parameter is dayEnd, second is dayStart.
    sqlite3_bind_double(stmt, 1, dayEnd);
    sqlite3_bind_double(stmt, 2, dayStart);

    // Get current time in Julian day (for sessions still active).
    double currentJulian = 0.0;
    {
        std::time_t now = std::time(nullptr);
        std::tm *lt = std::localtime(&now);
        int year = lt->tm_year + 1900;
        int month = lt->tm_mon + 1;
        int day = lt->tm_mday;
        int hour = lt->tm_hour;
        int minute = lt->tm_min;
        int second = lt->tm_sec;
        if (month <= 2) {
            year--;
            month += 12;
        }
        int A = year / 100;
        int B = 2 - A + (A / 4);
        double dayFraction = (hour + (minute / 60.0) + (second / 3600.0)) / 24.0;
        currentJulian = std::floor(365.25 * (year + 4716))
                      + std::floor(30.6001 * (month + 1))
                      + day + dayFraction + B - 1524.5;
    }

    // Process each session that overlaps the selected day.
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        double sessionStart = sqlite3_column_double(stmt, 0);
        // If endTime is NULL, use currentJulian.
        double sessionEnd = sqlite3_column_type(stmt, 1) == SQLITE_NULL
                            ? currentJulian
                            : sqlite3_column_double(stmt, 1);

        // Clip session times to the selected day.
        double effectiveStart = std::max(sessionStart, dayStart);
        double effectiveEnd = std::min(sessionEnd, dayEnd);
        if (effectiveEnd <= effectiveStart)
            continue; // No overlap with the selected day.

        // For each hour slot (0 to 23), compute overlap.
        for (int hour = 0; hour < 24; hour++) {
            double hourStart = dayStart + (hour / 24.0);
            double hourEnd = dayStart + ((hour + 1) / 24.0);
            double overlapStart = std::max(effectiveStart, hourStart);
            double overlapEnd = std::min(effectiveEnd, hourEnd);
            if (overlapEnd > overlapStart) {
                // Convert fractional days to seconds (1 day = 86400 seconds)
                double overlapSeconds = (overlapEnd - overlapStart) * 86400.0;
                usage[hour] += overlapSeconds;
            }
        }
    }
    sqlite3_finalize(stmt);

    // Normalize each hour's usage to a fraction of an hour (max 3600 seconds).
    for (int hour = 0; hour < 24; hour++) {
        double percent = usage[hour] / 3600.0;
        usage[hour] = (percent > 1.0) ? 1.0 : percent;
    }

    return usage;
}

// Helper: Maps a usage percentage (0.0 to 1.0) to a color (green low, red high).
ImVec4 getHeatMapColor(double percent)
{
    // Clamp percent to [0, 1]
    percent = std::max(0.0, std::min(1.0, percent));
    float r = static_cast<float>(percent);
    float g = static_cast<float>(1.0 - percent);
    return ImVec4(r, g, 0.0f, 1.0f);
}

// Draw the heat map using the computed hourly usage data.
void DrawHeatMap(const std::string& selectedDate)
{
    // Retrieve hourly usage data for the selected date.
    std::array<double, 24> hourlyUsage = computeHourlyUsage(selectedDate);

    // Set cell dimensions and spacing.
    const float cellWidth  = 30.0f;
    const float cellHeight = 50.0f;
    const float spacing    = 5.0f;

    // Get current cursor position as the top-left for drawing.
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Display a title.
    ImGui::Text("Hourly Activity Heat Map (%s)", selectedDate.c_str());
    ImGui::NewLine();
    ImGui::Spacing();

    // Loop over 24 hours.
    for (int hour = 0; hour < 24; hour++)
    {
        // Calculate the rectangle for the cell.
        ImVec2 cellPos = ImVec2(pos.x + hour * (cellWidth + spacing), pos.y);
        ImVec2 cellEnd = ImVec2(cellPos.x + cellWidth, cellPos.y + cellHeight);

        // Map the usage fraction to a color.
        ImVec4 col = getHeatMapColor(hourlyUsage[hour]);

        // Draw the filled rectangle with rounded corners.
        draw_list->AddRectFilled(cellPos, cellEnd, ImGui::GetColorU32(col), 4.0f);
        // Draw the cell border.
        draw_list->AddRect(cellPos, cellEnd, IM_COL32(255, 255, 255, 255), 4.0f);

        // Display the hour and the usage percentage (centered).
        char label[32];
        std::snprintf(label, sizeof(label), "%02d\n%.0f%%", hour, hourlyUsage[hour] * 100.0);
        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImVec2 textPos = ImVec2(cellPos.x + (cellWidth - textSize.x) * 0.5f,
                                 cellPos.y + (cellHeight - textSize.y) * 0.5f);
        draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), label);
    }

    // Advance the cursor to account for the drawn heat map.
    ImGui::Dummy(ImVec2(24 * (cellWidth + spacing), cellHeight));
}