/**
 * @file event_rate_chart.h
 * @brief Real-time event rate chart for displaying camera events per second
 */

#pragma once

#include <vector>
#include <deque>
#include <chrono>
#include <imgui.h>

namespace ui {

/**
 * @brief Chart configuration settings
 */
struct ChartSettings {
    float time_window = 60.0f;        // Time window in seconds
    float min_rate = 1000.0f;         // Minimum Y scale in events/s
    float max_rate = 1000000.0f;      // Maximum Y scale in events/s
    bool autoscale = true;             // Enable auto-scaling
};

/**
 * @brief Real-time chart displaying event rate over time
 *
 * Shows events per second with a configurable rolling window
 * Auto-scales vertically with configurable min/max limits
 */
class EventRateChart {
public:
    EventRateChart();
    ~EventRateChart() = default;

    /**
     * @brief Update the chart with new event count
     * @param event_count Total cumulative event count from camera
     */
    void update(uint64_t event_count);

    /**
     * @brief Render the chart using ImGui
     * @param width Chart width in pixels
     * @param height Chart height in pixels
     */
    void render(float width = -1.0f, float height = 100.0f);

    /**
     * @brief Clear all data and reset the chart
     */
    void reset();

    /**
     * @brief Get current event rate in events/second
     */
    float get_current_rate() const { return current_rate_; }

    /**
     * @brief Set chart configuration settings
     */
    void set_settings(const ChartSettings& settings) { settings_ = settings; }

    /**
     * @brief Get current chart settings
     */
    const ChartSettings& get_settings() const { return settings_; }

    /**
     * @brief Get mutable reference to chart settings for UI binding
     */
    ChartSettings& get_mutable_settings() { return settings_; }

private:
    struct DataPoint {
        std::chrono::steady_clock::time_point timestamp;
        float rate;  // Events per second
    };

    // Chart data
    std::deque<DataPoint> data_points_;
    static constexpr size_t MAX_POINTS = 6000;  // Up to 600 seconds at 10Hz update

    // Chart settings
    ChartSettings settings_;

    // Rate calculation
    uint64_t last_event_count_ = 0;
    std::chrono::steady_clock::time_point last_update_time_;
    float current_rate_ = 0.0f;

    // Auto-scaling
    float max_rate_ = 1000.0f;
    float y_scale_ = 1000.0f;
    float y_scale_target_ = 1000.0f;

    // Update control
    std::chrono::steady_clock::time_point last_chart_update_;
    static constexpr int UPDATE_INTERVAL_MS = 100;  // Update chart at 10Hz

    /**
     * @brief Clean old data points outside the time window
     */
    void cleanup_old_points();

    /**
     * @brief Update Y-axis scaling based on current data
     */
    void update_scaling();
};

} // namespace ui