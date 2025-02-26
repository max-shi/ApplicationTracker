#include "heatmap.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <iomanip>

#include "functions.h"
#include "database.h"
#include <sqlite3.h>
#include <imgui.h>
#include <iostream>
#include <unordered_map>

std::vector<ApplicationData> getTopApplicationsTimeRange(double queryStart, double queryEnd) {
    std::vector<ApplicationData> results;
    sqlite3* db = getDatabase();
    if (!db)
        return results;

    // SQL query: select sessions that overlap with the given time range.
    std::string sql =
        "SELECT processName, startTime, endTime "
        "FROM ActivitySession "
        "WHERE startTime < ? AND (endTime > ? OR endTime IS NULL);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement in getTopApplicationsTimeRange: "
                  << sqlite3_errmsg(db) << std::endl;
        return results;
    }

    // Bind parameters:
    // Parameter 1: queryEnd (session must have started before the end of our range)
    // Parameter 2: queryStart (session must end after the start of our range)
    sqlite3_bind_double(stmt, 1, queryEnd);
    sqlite3_bind_double(stmt, 2, queryStart);

    // Use an unordered_map to accumulate usage per process.
    std::unordered_map<std::string, double> usageMap;
    double currentJulian = getCurrentJulianDay(); // For sessions still active

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        // Column 0: processName, Column 1: startTime, Column 2: endTime.
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        std::string processName = text ? reinterpret_cast<const char*>(text) : "";
        double sessionStart = sqlite3_column_double(stmt, 1);
        double sessionEnd = (sqlite3_column_type(stmt, 2) == SQLITE_NULL)
                            ? currentJulian
                            : sqlite3_column_double(stmt, 2);

        // Compute effective overlap with the query range.
        double effectiveStart = std::max(sessionStart, queryStart);
        double effectiveEnd = std::min(sessionEnd, queryEnd);

        if (effectiveEnd > effectiveStart) {
            // Convert the fractional days difference to seconds.
            double overlapSeconds = (effectiveEnd - effectiveStart) * 86400.0;
            usageMap[processName] += overlapSeconds;
        }
    }
    sqlite3_finalize(stmt);

    // Convert the map to a vector of ApplicationData.
    for (const auto& entry : usageMap) {
        ApplicationData appData;
        appData.processName = entry.first;
        appData.totalTime = entry.second;
        results.push_back(appData);
    }

    // Optionally sort by totalTime (largest usage first)
    std::sort(results.begin(), results.end(), [](const ApplicationData& a, const ApplicationData& b) {
        return a.totalTime > b.totalTime;
    });

    return results;
}

// Get the application data for a specific hour
std::vector<ApplicationData> getHourlyApplicationData(const std::string& selectedDate, int hour) {
    // Convert hour to Julian day time range
    double dayStart = getJulianDayFromDate(selectedDate);
    double hourStart = dayStart + (hour / 24.0);
    double hourEnd = dayStart + ((hour + 1) / 24.0);

    // Get applications used in this hour
    // Implementation would be similar to getTopApplications but with time constraints
    // This is a placeholder - you'll need to implement this based on your database schema
    return getTopApplicationsTimeRange(hourStart, hourEnd);
}

// Compute hourly usage with app category information
struct HourlyUsageData {
    double totalUsage;                    // Total usage as fraction of hour (0.0-1.0)
    std::vector<ApplicationData> apps;    // Top apps used in this hour
};

std::array<HourlyUsageData, 24> computeDetailedHourlyUsage(const std::string& selectedDate) {
    std::array<HourlyUsageData, 24> usage{};

    // Initialize array
    for (int hour = 0; hour < 24; hour++) {
        usage[hour].totalUsage = 0.0;
    }

    // Determine the start and end (Julian day) of the selected date
    double dayStart = getJulianDayFromDate(selectedDate);
    std::string nextDate = getNextDate(selectedDate);
    double dayEnd = getJulianDayFromDate(nextDate);

    // SQL: Get sessions overlapping the selected day
    std::string sql =
        "SELECT startTime, endTime FROM ActivitySession "
        "WHERE startTime < ? AND (endTime > ? OR endTime IS NULL);";

    sqlite3* db = getDatabase();
    if (!db) {
        return usage;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return usage;
    }

    // Bind parameters: first parameter is dayEnd, second is dayStart
    sqlite3_bind_double(stmt, 1, dayEnd);
    sqlite3_bind_double(stmt, 2, dayStart);

    // Get current time in Julian day (for sessions still active)
    double currentJulian = getCurrentJulianDay();

    // Process each session that overlaps the selected day
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        double sessionStart = sqlite3_column_double(stmt, 0);
        // If endTime is NULL, use currentJulian
        double sessionEnd = sqlite3_column_type(stmt, 1) == SQLITE_NULL
                            ? currentJulian
                            : sqlite3_column_double(stmt, 1);

        // Clip session times to the selected day
        double effectiveStart = std::max(sessionStart, dayStart);
        double effectiveEnd = std::min(sessionEnd, dayEnd);
        if (effectiveEnd <= effectiveStart)
            continue; // No overlap with the selected day

        // For each hour slot (0 to 23), compute overlap
        for (int hour = 0; hour < 24; hour++) {
            double hourStart = dayStart + (hour / 24.0);
            double hourEnd = dayStart + ((hour + 1) / 24.0);
            double overlapStart = std::max(effectiveStart, hourStart);
            double overlapEnd = std::min(effectiveEnd, hourEnd);
            if (overlapEnd > overlapStart) {
                // Convert fractional days to seconds (1 day = 86400 seconds)
                double overlapSeconds = (overlapEnd - overlapStart) * 86400.0;
                usage[hour].totalUsage += overlapSeconds / 3600.0; // Convert to fraction of hour
            }
        }
    }
    sqlite3_finalize(stmt);

    // Cap usage at 1.0 (100% of hour)
    for (int hour = 0; hour < 24; hour++) {
        usage[hour].totalUsage = std::min(1.0, usage[hour].totalUsage);
        // Fetch detailed app usage for each hour (placeholder)
        // In a real implementation, you'd query the database for this
        if (usage[hour].totalUsage > 0) {
            usage[hour].apps = getHourlyApplicationData(selectedDate, hour);
        }
    }

    return usage;
}

// Color palette for different app categories
// These are more subtle, Apple-like colors
struct CategoryColor {
    const char* category;
    ImU32 color;
};

const std::array<CategoryColor, 7> categoryColors = {{
    {"Productivity", IM_COL32(88, 86, 214, 255)},     // Purple
    {"Entertainment", IM_COL32(52, 170, 220, 255)},   // Blue
    {"Social", IM_COL32(90, 200, 250, 255)},          // Light Blue
    {"Communication", IM_COL32(255, 149, 0, 255)},    // Orange
    {"Reading", IM_COL32(88, 189, 92, 255)},          // Green
    {"Creativity", IM_COL32(255, 45, 85, 255)},       // Red
    {"Other", IM_COL32(142, 142, 147, 255)}           // Gray
}};

// Get color for a specific app (based on category)
ImU32 getAppColor(const std::string& appName) {
    // In a real implementation, you would categorize apps
    // For now, we'll use a simple hash-based approach for demonstration
    size_t hash = std::hash<std::string>{}(appName);
    return categoryColors[hash % categoryColors.size()].color;
}

// Modernized heat map that resembles Apple's Screen Time
void DrawHeatMap(const std::string& selectedDate) {
    ImGui::Begin("Activity Timeline");

    // Get data for the selected date
    auto hourlyData = computeDetailedHourlyUsage(selectedDate);

    // Instead of normalizing relative to the maximum observed usage,
    // we use an absolute scale where 1.0 equals a full hour (60 minutes).
    const double maxUsage = 1.0;

    // UI Constants
    const float kBarHeight = 150.0f;            // Maximum height of bars
    const float kBarWidth = 20.0f;              // Width of each bar
    const float kBarSpacing = 8.0f;             // Space between bars
    const float kAxisPadding = 50.0f;           // Space for axis labels
    const float kTimelineWidth = 24 * (kBarWidth + kBarSpacing) - kBarSpacing; // Total width

    // Calculate area for the chart
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 chartStart = ImVec2(pos.x + kAxisPadding, pos.y + 40.0f);
    ImVec2 chartEnd = ImVec2(chartStart.x + kTimelineWidth, chartStart.y + kBarHeight + kAxisPadding);

    // Get draw list for custom rendering
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Draw title
    {
        std::string title = "Screen Time · " + selectedDate;
        ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
        float titleX = chartStart.x + (kTimelineWidth - titleSize.x) * 0.5f;
        draw_list->AddText(ImVec2(titleX, pos.y), IM_COL32(60, 60, 60, 255), title.c_str());
        ImGui::Dummy(ImVec2(0, titleSize.y + 10)); // Space after title
        pos.y += titleSize.y + 10;
    }

    // Draw y-axis labels
    {
        char label[32];
        snprintf(label, sizeof(label), "60 min");
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        draw_list->AddText(
            ImVec2(chartStart.x - labelSize.x - 5, chartStart.y),
            IM_COL32(120, 120, 120, 255), label);

        snprintf(label, sizeof(label), "30 min");
        labelSize = ImGui::CalcTextSize(label);
        draw_list->AddText(
            ImVec2(chartStart.x - labelSize.x - 5, chartStart.y + kBarHeight * 0.5f),
            IM_COL32(120, 120, 120, 255), label);

        snprintf(label, sizeof(label), "0 min");
        labelSize = ImGui::CalcTextSize(label);
        draw_list->AddText(
            ImVec2(chartStart.x - labelSize.x - 5, chartStart.y + kBarHeight),
            IM_COL32(120, 120, 120, 255), label);
    }

    // Draw horizontal grid lines (subtle)
    {
        // 60 minutes line
        draw_list->AddLine(
            ImVec2(chartStart.x - 2, chartStart.y),
            ImVec2(chartEnd.x, chartStart.y),
            IM_COL32(200, 200, 200, 100), 1.0f);

        // 30 minutes line
        draw_list->AddLine(
            ImVec2(chartStart.x - 2, chartStart.y + kBarHeight * 0.5f),
            ImVec2(chartEnd.x, chartStart.y + kBarHeight * 0.5f),
            IM_COL32(200, 200, 200, 100), 1.0f);

        // 0 minutes line
        draw_list->AddLine(
            ImVec2(chartStart.x - 2, chartStart.y + kBarHeight),
            ImVec2(chartEnd.x, chartStart.y + kBarHeight),
            IM_COL32(200, 200, 200, 100), 1.0f);
    }

    // Track if any hour is hovered
    int hoveredHour = -1;

    // Draw bars for each hour
    for (int hour = 0; hour < 24; hour++) {
        float x = chartStart.x + hour * (kBarWidth + kBarSpacing);
        // Scale based on absolute usage (where full hour = 1.0)
        float barHeight = kBarHeight * hourlyData[hour].totalUsage;

        // Bar position
        ImVec2 barTop = ImVec2(x, chartStart.y + kBarHeight - barHeight);
        ImVec2 barBottom = ImVec2(x + kBarWidth, chartStart.y + kBarHeight);

        // Check if this bar is hovered
        ImVec2 mousePos = ImGui::GetMousePos();
        bool isHovered = mousePos.x >= x && mousePos.x <= x + kBarWidth &&
                         mousePos.y >= barTop.y && mousePos.y <= barBottom.y;

        if (isHovered) {
            hoveredHour = hour;
        }

        // Background of the bar (subtle gray)
        draw_list->AddRectFilled(
            ImVec2(x, chartStart.y),
            ImVec2(x + kBarWidth, chartStart.y + kBarHeight),
            IM_COL32(240, 240, 240, 255),
            3.0f);

        // Draw activity segments (apps used in this hour)
        if (barHeight > 0) {
            const auto& apps = hourlyData[hour].apps;
            if (!apps.empty()) {
                // Calculate proportional heights
                float totalTime = 0;
                for (const auto& app : apps) {
                    totalTime += app.totalTime;
                }

                float currentHeight = 0;
                for (const auto& app : apps) {
                    float appHeight = barHeight * (app.totalTime / totalTime);

                    // Draw app segment
                    ImVec2 segmentTop = ImVec2(x, chartStart.y + kBarHeight - currentHeight - appHeight);
                    ImVec2 segmentBottom = ImVec2(x + kBarWidth, chartStart.y + kBarHeight - currentHeight);

                    draw_list->AddRectFilled(
                        segmentTop,
                        segmentBottom,
                        getAppColor(app.processName),
                        isHovered ? 0.0f : 3.0f); // Flatten corners when hovered

                    currentHeight += appHeight;
                }
            } else {
                // If we don't have app details, just draw a uniform bar
                draw_list->AddRectFilled(
                    barTop,
                    barBottom,
                    IM_COL32(90, 200, 250, 255), // Default blue
                    3.0f);
            }
        }

        // Add a subtle border to each bar
        draw_list->AddRect(
            ImVec2(x, chartStart.y),
            ImVec2(x + kBarWidth, chartStart.y + kBarHeight),
            IM_COL32(200, 200, 200, 150),
            3.0f, 0, 1.0f);

        // Highlight border for hovered bar
        if (isHovered) {
            draw_list->AddRect(
                ImVec2(x - 1, chartStart.y - 1),
                ImVec2(x + kBarWidth + 1, chartStart.y + kBarHeight + 1),
                IM_COL32(50, 50, 50, 200),
                3.0f, 0, 2.0f);
        }
    }

    // Draw x-axis labels (every 3 hours)
    for (int hour = 0; hour < 24; hour += 3) {
        char label[8];
        if (hour == 0) {
            snprintf(label, sizeof(label), "12 AM");
        } else if (hour == 12) {
            snprintf(label, sizeof(label), "12 PM");
        } else if (hour < 12) {
            snprintf(label, sizeof(label), "%d AM", hour);
        } else {
            snprintf(label, sizeof(label), "%d PM", hour - 12);
        }

        float x = chartStart.x + hour * (kBarWidth + kBarSpacing) + kBarWidth * 0.5f;
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        draw_list->AddText(
            ImVec2(x - labelSize.x * 0.5f, chartStart.y + kBarHeight + 5),
            IM_COL32(120, 120, 120, 255), label);
    }

    // Reserve space for the chart in ImGui layout
    ImGui::Dummy(ImVec2(kTimelineWidth + kAxisPadding, kBarHeight + kAxisPadding + 20));

    // Display details for hovered hour
    if (hoveredHour >= 0) {
        ImGui::Separator();

        // Format time range
        char timeRange[32];
        if (hoveredHour == 0) {
            snprintf(timeRange, sizeof(timeRange), "12:00 AM - 1:00 AM");
        } else if (hoveredHour == 12) {
            snprintf(timeRange, sizeof(timeRange), "12:00 PM - 1:00 PM");
        } else if (hoveredHour < 12) {
            snprintf(timeRange, sizeof(timeRange), "%d:00 AM - %d:00 AM", hoveredHour, hoveredHour + 1);
        } else {
            snprintf(timeRange, sizeof(timeRange), "%d:00 PM - %d:00 PM", hoveredHour - 12, hoveredHour - 12 + 1);
        }

        // Title with usage time
        char title[64];
        int minutes = static_cast<int>(hourlyData[hoveredHour].totalUsage * 60);
        snprintf(title, sizeof(title), "%s · %d min", timeRange, minutes);
        ImGui::Text("%s", title);

        // App details
        ImGui::Spacing();
        const auto& apps = hourlyData[hoveredHour].apps;
        if (!apps.empty()) {
            if (ImGui::BeginTable("HourDetails", 2, ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("App", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 100.0f);

                for (const auto& app : apps) {
                    ImGui::TableNextRow();

                    // App name column
                    ImGui::TableSetColumnIndex(0);
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

                    // Color indicator
                    draw_list->AddRectFilled(
                        ImVec2(cursorPos.x, cursorPos.y + 2),
                        ImVec2(cursorPos.x + 10, cursorPos.y + ImGui::GetTextLineHeight() - 2),
                        getAppColor(app.processName),
                        2.0f);

                    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 15, cursorPos.y));
                    ImGui::Text("%s", app.processName.c_str());

                    // Time column
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", formatTime(app.totalTime).c_str());
                }

                ImGui::EndTable();
            }
        } else {
            ImGui::Text("No detailed app data available for this hour");
        }
    }

    // Draw legend for app categories
    ImGui::Separator();
    ImGui::Text("App Categories");
    ImGui::Spacing();

    // Create a simple grid for the legend
    float windowWidth = ImGui::GetContentRegionAvail().x;
    int itemsPerRow = static_cast<int>(windowWidth / 200.0f);
    itemsPerRow = std::max(1, itemsPerRow);

    int col = 0;
    for (const auto& category : categoryColors) {
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();

        // Color indicator
        draw_list->AddRectFilled(
            ImVec2(cursorPos.x, cursorPos.y + 2),
            ImVec2(cursorPos.x + 15, cursorPos.y + ImGui::GetTextLineHeight() - 2),
            category.color,
            3.0f);

        ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 20, cursorPos.y));
        ImGui::Text("%s", category.category);

        // Handle columns
        col++;
        if (col < itemsPerRow) {
            ImGui::SameLine(col * (windowWidth / itemsPerRow));
        } else {
            col = 0;
        }
    }

    ImGui::End();
}

