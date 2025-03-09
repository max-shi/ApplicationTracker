# Application Tracker

A Windows desktop application that tracks website and application usage. This tool logs user sessions in an SQLite database and presents activity data with an interactive IMGUI-based interface. In the long term, the application is designed to integrate with a Chrome extension to provide in-depth tracking of user activity.
Libraries have been included for ease of building (may be deprecated in the future)

## Overview

The Application Tracker helps users understand their daily activity patterns by tracking:
- **Current Application:** The application currently being tracked.
- **Total Time Tracked:** Cumulative time usage.
- **Top 10 Applications:** Ranked by usage (with views for total time, daily averages, or specific day totals).
- **Pie Chart:** Displays the top 10 applications (with >1.5% runtime) as a percentage of total usage.
- **Calendar:** Allows the user to select a specific date to view detailed usage statistics.
- **Heatmap:** Shows hourly usage percentages, with an interactive activity breakdown that can be locked on click.
- **Category Assignment Pane:** Lists all processes (aggregated from all-time usage) and enables the user to assign custom categories. These categories are then integrated into the activity timeline.

## Features

- **Session Logging:**  
  Records sessions in an SQLite database using C++.

- **Multiple Data Views:**  
  - **Controls Pane:** Filter data by date and switch between All-Time, Daily Total, and Daily Average modes.
  - **Top 10 Applications:** View the most used applications with customizable metrics.
  - **Pie Chart & Heatmap:** Visualize application usage with interactive charts.
  - **Calendar View:** Select dates to display specific usage data.
  - **Category Assignment:** Reassign applications to custom categories via an interactive table.

- **Extensibility:**  
  Future integration with a Chrome extension for comprehensive tracking of website usage alongside desktop applications.

## Technology Stack

- **Language:** C++  
- **IDE:** CLion  
- **Database:** SQLite  
- **User Interface:** IMGUI (using OpenGL3 and Win32 backends)  
- **Graphics:** OpenGL via gl3w  
- **Platform:** Windows

## Prerequisites

- **CLion IDE** (or any C++ IDE that supports CMake)
- **C++ 17 (or later) Compiler**
- **SQLite Development Libraries**
- **OpenGL Development Libraries** (gl3w, etc.)
- **Windows SDK**

## Installation and Build Instructions

1. **Clone the Repository:**

    ```bash
    git clone https://github.com/max-shi/tracker.git
    ```

2. **Open the Project in CLion:**
   - Launch CLion and choose **File > Open...**, then select the project directory.

3. **Configure CMake:**
   - Ensure that the `CMakeLists.txt` file includes the necessary include paths and links libraries for SQLite, IMGUI, OpenGL (gl3w), and the Windows SDK.

4. **Build the Project:**
   - Use the build button in CLion (or press `Ctrl+F9`) to compile the project.

5. **Run the Application:**
   - After a successful build, run the application directly from CLion. The program will create or connect to an SQLite database (e.g., `activity_log.db`) in the project directory.

![image](https://github.com/user-attachments/assets/2771d370-77b0-42cb-a6d8-526668dd971a)


## Running the Application

Upon launch, the main window will display multiple IMGUI panes:

- **Controls Pane:**  
  Adjust the date filter, switch between tracking modes (All-Time, Daily Total, Daily Average), and navigate through dates.

- **Total Time Tracked Pane:**  
  Displays the overall time tracked (all-time or for a specific day).

- **Top 10 Applications Pane:**  
  Lists the top 10 used applications. The view can be modified to show total time, daily averages, or specific day usage.

- **Pie Chart Pane:**  
  Visualizes the usage distribution for the top applications.

- **Calendar Pane:**  
  Allows date selection to update displayed statistics.

- **Heatmap Pane:**  
  Displays hourly usage percentages and an interactive breakdown that can be locked on click.

- **App Category Assignment Pane:**  
  Lists all processes aggregated from all-time usage and allows assignment of custom categories. Changes here immediately reflect in the activity timeline.
