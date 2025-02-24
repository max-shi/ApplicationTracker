#include "pie_chart.h"
#include <cmath>
#include <imgui_internal.h>

void DrawPieChart(const std::vector<ApplicationData>& topApps,
                  double overallTime,
                  const ImVec2& center,
                  float radius,
                  const std::string& highlightProcess,
                  std::vector<std::pair<PieSlice, std::pair<float, float>>>& outSliceAngles)
{
    outSliceAngles.clear();

    // Sum total time for top apps.
    double topAppsSum = 0.0;
    for (const auto &app : topApps)
        topAppsSum += app.totalTime;
    // Compute "base" other time: sessions not in topApps.
    double baseOtherTime = overallTime - topAppsSum;

    // Separate topApps into those above the threshold and those below.
    const double thresholdPercent = 1.5;
    std::vector<PieSlice> bigSlices;
    double aggregatedSmall = 0.0;

    for (size_t i = 0; i < topApps.size(); i++) {
        double percent = (overallTime > 0) ? (topApps[i].totalTime / overallTime * 100.0) : 0.0;
        // Choose a color (this is a simple formula; customize as needed).
        ImU32 color = IM_COL32(50 + (i * 20) % 205, 100 + (i * 30) % 155, 150 + (i * 40) % 105, 255);
        if (percent < thresholdPercent) {
            aggregatedSmall += topApps[i].totalTime;
        } else {
            bigSlices.push_back({ topApps[i].processName, topApps[i].totalTime, color });
        }
    }

    // Combine aggregated small slices with base "other" time.
    double finalOther = aggregatedSmall + baseOtherTime;

    std::vector<PieSlice> slicesToDraw = bigSlices;
    if (finalOther > 0.0)
        slicesToDraw.push_back({ "Other", finalOther, IM_COL32(120, 120, 120, 255) });

    // Get the current window's draw list.
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float startAngle = 0.0f;
    const int numSegments = 32; // More segments = smoother arc.

    for (const auto& slice : slicesToDraw)
    {
        float sliceAngle = 2 * IM_PI * (slice.value / overallTime);
        float endAngle = startAngle + sliceAngle;

        // If this slice is to be highlighted, darken its color.
        ImU32 drawColor = slice.color;
        if (!highlightProcess.empty() && slice.label == highlightProcess) {
            int r = drawColor & 0xFF;
            int g = (drawColor >> 8) & 0xFF;
            int b = (drawColor >> 16) & 0xFF;
            int a = (drawColor >> 24) & 0xFF;
            r = (int)(r * 0.8f);
            g = (int)(g * 0.8f);
            b = (int)(b * 0.8f);
            drawColor = IM_COL32(r, g, b, a);
        }

        // Compute points along the arc.
        ImVector<ImVec2> points;
        points.push_back(center);
        for (int i = 0; i <= numSegments; i++) {
            float angle = startAngle + (endAngle - startAngle) * (i / (float)numSegments);
            points.push_back(ImVec2(center.x + cosf(angle) * radius,
                                      center.y + sinf(angle) * radius));
        }

        // Draw the filled slice.
        draw_list->AddConvexPolyFilled(points.Data, points.Size, drawColor);
        // Optionally draw an outline (thicker if highlighted).
        float thickness = (!highlightProcess.empty() && slice.label == highlightProcess) ? 2.0f : 1.0f;
        draw_list->AddPolyline(points.Data, points.Size, IM_COL32(0, 0, 0, 255), true, thickness);

        // Record the slice boundaries for later hover detection.
        outSliceAngles.push_back({ slice, { startAngle, endAngle } });

        startAngle = endAngle;
    }
}
