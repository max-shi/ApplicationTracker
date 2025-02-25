//
// Created by Max on 26/02/2025.
//

#ifndef HEATMAP_H
#define HEATMAP_H
#include <imgui.h>
#include <string>


ImVec4 getHeatMapColor(double percent);
void DrawHeatMap(const std::string& selectedDate);
std::array<double, 24> computeHourlyUsage(const std::string& selectedDate);




#endif //HEATMAP_H
