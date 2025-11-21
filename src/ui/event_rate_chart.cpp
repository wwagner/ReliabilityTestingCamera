/**
 * @file event_rate_chart.cpp
 * @brief Implementation of real-time event rate chart
 */

#include "ui/event_rate_chart.h"
#include <algorithm>
#include <cmath>

namespace ui {

EventRateChart::EventRateChart() {
    last_update_time_ = std::chrono::steady_clock::now();
    last_chart_update_ = last_update_time_;
    // Initialize with default settings
    settings_ = ChartSettings();
    reset();
}

void EventRateChart::update(uint64_t event_count) {
    auto now = std::chrono::steady_clock::now();

    // Calculate time delta
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_update_time_).count();

    // Skip if updating too frequently
    if (time_diff < UPDATE_INTERVAL_MS) {
        return;
    }

    // Calculate event rate
    if (last_event_count_ > 0 && event_count >= last_event_count_) {
        uint64_t event_diff = event_count - last_event_count_;
        float time_seconds = time_diff / 1000.0f;
        current_rate_ = event_diff / time_seconds;

        // Add data point
        data_points_.push_back({now, current_rate_});

        // Limit number of points
        if (data_points_.size() > MAX_POINTS) {
            data_points_.pop_front();
        }
    }

    last_event_count_ = event_count;
    last_update_time_ = now;
    last_chart_update_ = now;

    // Clean old points and update scaling
    cleanup_old_points();
    update_scaling();
}

void EventRateChart::render(float width, float height) {
    if (data_points_.empty()) {
        ImGui::Text("No event data");
        return;
    }

    // Prepare data for plotting
    std::vector<float> values;
    values.reserve(data_points_.size());

    for (const auto& point : data_points_) {
        values.push_back(point.rate);
    }

    // Calculate time range
    auto now = std::chrono::steady_clock::now();
    float time_range = 0.0f;
    if (data_points_.size() > 1) {
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            data_points_.back().timestamp - data_points_.front().timestamp).count();
        time_range = time_diff / 1000.0f;
    }

    // Format overlay text
    char overlay_text[256];
    snprintf(overlay_text, sizeof(overlay_text),
             "Rate: %.1f kev/s | Time: %.1fs | Scale: %.1f kev/s",
             current_rate_ / 1000.0f,
             std::min(time_range, settings_.time_window),
             y_scale_ / 1000.0f);

    // Draw the plot
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));

    ImGui::PlotLines("##EventRate",
                     values.data(),
                     static_cast<int>(values.size()),
                     0,
                     overlay_text,
                     0.0f,
                     y_scale_,
                     ImVec2(width, height));

    ImGui::PopStyleColor(2);

    // Draw grid lines and labels
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    canvas_pos.y -= height;  // Adjust for plot height

    // Draw horizontal grid lines
    ImU32 grid_color = IM_COL32(100, 100, 100, 100);
    int num_grid_lines = 5;

    for (int i = 0; i <= num_grid_lines; ++i) {
        float y = canvas_pos.y + (height * i / num_grid_lines);
        draw_list->AddLine(ImVec2(canvas_pos.x, y),
                          ImVec2(canvas_pos.x + width, y),
                          grid_color);

        // Add Y-axis labels
        float value = y_scale_ * (1.0f - (float)i / num_grid_lines);
        char label[32];
        snprintf(label, sizeof(label), "%.1fk", value / 1000.0f);
        draw_list->AddText(ImVec2(canvas_pos.x - 35, y - 7),
                          IM_COL32(200, 200, 200, 255), label);
    }
}

void EventRateChart::reset() {
    data_points_.clear();
    last_event_count_ = 0;
    current_rate_ = 0.0f;
    max_rate_ = settings_.min_rate;
    y_scale_ = settings_.min_rate;
    y_scale_target_ = settings_.min_rate;
    last_update_time_ = std::chrono::steady_clock::now();
    last_chart_update_ = last_update_time_;
}

void EventRateChart::cleanup_old_points() {
    auto now = std::chrono::steady_clock::now();

    // Remove points older than configured time window
    while (!data_points_.empty()) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - data_points_.front().timestamp).count();

        if (age > settings_.time_window) {
            data_points_.pop_front();
        } else {
            break;
        }
    }
}

void EventRateChart::update_scaling() {
    if (data_points_.empty()) {
        return;
    }

    if (settings_.autoscale) {
        // Find maximum rate in current data
        float current_max = settings_.min_rate;
        for (const auto& point : data_points_) {
            current_max = std::max(current_max, point.rate);
        }

        // Add 20% headroom
        y_scale_target_ = current_max * 1.2f;

        // Clamp to configured min/max
        y_scale_target_ = std::max(y_scale_target_, settings_.min_rate);
        y_scale_target_ = std::min(y_scale_target_, settings_.max_rate);

        // Smooth scaling transition
        float scale_diff = y_scale_target_ - y_scale_;
        y_scale_ += scale_diff * 0.1f;  // Smooth transition

        // Snap to target if close enough
        if (std::abs(scale_diff) < 10.0f) {
            y_scale_ = y_scale_target_;
        }
    } else {
        // Use fixed scale (max_rate) when autoscale is disabled
        y_scale_ = settings_.max_rate;
        y_scale_target_ = settings_.max_rate;
    }
}

} // namespace ui