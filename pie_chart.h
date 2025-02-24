#ifndef PIE_CHART_H
#define PIE_CHART_H

#include <vector>
#include <string>
#include "imgui.h"
#include "functions.h" // For ApplicationData and formatTime()

// A simple structure representing a pie slice.
struct PieSlice {
    std::string label;
    double value;
    ImU32 color;
};

/// Draws a pie chart based on the top applications and overall tracked time.
/// The slice corresponding to highlightProcess is drawn with a darkened color.
/// The function also outputs each slice’s start and end angle (in radians) for hover detection.
/// @param topApps Vector of ApplicationData (top apps).
/// @param overallTime Total tracked time (in seconds).
/// @param center Center of the pie chart in screen coordinates.
/// @param radius Radius of the pie chart.
/// @param highlightProcess Process name to highlight (if any).
/// @param outSliceAngles On return, each element contains a PieSlice and a pair (startAngle, endAngle).
void DrawPieChart(const std::vector<ApplicationData>& topApps,
                  double overallTime,
                  const ImVec2& center,
                  float radius,
                  const std::string& highlightProcess,
                  std::vector<std::pair<PieSlice, std::pair<float, float>>>& outSliceAngles);

#endif // PIE_CHART_H
