#include "scattering_analyzer.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <iostream>

ScatteringAnalyzer::ScatteringAnalyzer() : analyzing_(false) {
}

bool ScatteringAnalyzer::start_analysis(const cv::Mat& reference_image) {
    if (reference_image.empty()) {
        std::cerr << "ScatteringAnalyzer: Cannot start with empty reference image" << std::endl;
        return false;
    }

    if (reference_image.type() != CV_8UC1) {
        std::cerr << "ScatteringAnalyzer: Reference image must be single-channel grayscale" << std::endl;
        return false;
    }

    // Store reference image
    reference_image_ = reference_image.clone();

    // Initialize data structures
    data_.scattering_mask = cv::Mat::zeros(reference_image.size(), CV_8UC1);
    data_.scattering_count = cv::Mat::zeros(reference_image.size(), CV_32SC1);
    data_.scattering_heatmap = cv::Mat::zeros(reference_image.size(), CV_8UC1);

    // Reset counters
    data_.current_scattering_pixels = 0;
    data_.current_scattering_percentage = 0.0f;
    data_.frames_analyzed = 0;
    data_.max_scattering_count = 0;
    data_.hot_spot_location = cv::Point(0, 0);
    data_.total_scattering_events = 0;
    data_.average_scattering_per_frame = 0.0f;

    analyzing_ = true;
    std::cout << "Scattering analysis started with reference image "
              << reference_image.cols << "x" << reference_image.rows << std::endl;
    return true;
}

bool ScatteringAnalyzer::analyze_frame(const cv::Mat& live_image) {
    if (!analyzing_) {
        std::cerr << "ScatteringAnalyzer: Analysis not started" << std::endl;
        return false;
    }

    if (live_image.empty() || live_image.size() != reference_image_.size()) {
        std::cerr << "ScatteringAnalyzer: Live image size mismatch" << std::endl;
        return false;
    }

    // Detect scattering pixels: active in live but NOT in reference
    // scattering_mask = live AND NOT reference
    cv::Mat not_reference;
    cv::bitwise_not(reference_image_, not_reference);
    cv::bitwise_and(live_image, not_reference, data_.scattering_mask);

    // Count scattering pixels in current frame
    data_.current_scattering_pixels = cv::countNonZero(data_.scattering_mask);
    int total_pixels = live_image.cols * live_image.rows;
    data_.current_scattering_percentage =
        (float)data_.current_scattering_pixels / total_pixels * 100.0f;

    // Update temporal tracking
    // Increment count for each pixel that is scattering
    for (int y = 0; y < data_.scattering_mask.rows; ++y) {
        const uint8_t* mask_row = data_.scattering_mask.ptr<uint8_t>(y);
        int32_t* count_row = data_.scattering_count.ptr<int32_t>(y);

        for (int x = 0; x < data_.scattering_mask.cols; ++x) {
            if (mask_row[x] > 0) {
                count_row[x]++;
            }
        }
    }

    data_.frames_analyzed++;
    update_statistics();
    update_heatmap();
    detect_hot_spots();

    return true;
}

void ScatteringAnalyzer::stop_analysis() {
    analyzing_ = false;
    std::cout << "Scattering analysis stopped after " << data_.frames_analyzed << " frames" << std::endl;
    std::cout << "Total scattering events: " << data_.total_scattering_events << std::endl;
    std::cout << "Average per frame: " << data_.average_scattering_per_frame << std::endl;
}

void ScatteringAnalyzer::reset_temporal_data() {
    if (!analyzing_) return;

    data_.scattering_count = cv::Mat::zeros(reference_image_.size(), CV_32SC1);
    data_.scattering_heatmap = cv::Mat::zeros(reference_image_.size(), CV_8UC1);
    data_.frames_analyzed = 0;
    data_.max_scattering_count = 0;
    data_.total_scattering_events = 0;
    data_.average_scattering_per_frame = 0.0f;

    std::cout << "Scattering temporal data reset" << std::endl;
}

cv::Mat ScatteringAnalyzer::create_scattering_visualization(
    const cv::Mat& live_image,
    const cv::Scalar& highlight_color
) const {
    if (live_image.empty()) {
        return cv::Mat();
    }

    // Create color image from live grayscale
    cv::Mat visualization;
    if (live_image.channels() == 1) {
        cv::cvtColor(live_image, visualization, cv::COLOR_GRAY2BGR);
    } else {
        visualization = live_image.clone();
    }

    // Highlight scattering pixels
    for (int y = 0; y < data_.scattering_mask.rows; ++y) {
        const uint8_t* mask_row = data_.scattering_mask.ptr<uint8_t>(y);
        cv::Vec3b* vis_row = visualization.ptr<cv::Vec3b>(y);

        for (int x = 0; x < data_.scattering_mask.cols; ++x) {
            if (mask_row[x] > 0) {
                // Replace pixel with highlight color
                vis_row[x] = cv::Vec3b(
                    static_cast<uint8_t>(highlight_color[0]),
                    static_cast<uint8_t>(highlight_color[1]),
                    static_cast<uint8_t>(highlight_color[2])
                );
            }
        }
    }

    return visualization;
}

cv::Mat ScatteringAnalyzer::create_heatmap_visualization() const {
    if (data_.scattering_count.empty() || data_.max_scattering_count == 0) {
        return cv::Mat::zeros(reference_image_.size(), CV_8UC3);
    }

    // Normalize count to 0-255 range
    cv::Mat normalized;
    double max_val = static_cast<double>(data_.max_scattering_count);
    data_.scattering_count.convertTo(normalized, CV_8UC1, 255.0 / max_val);

    // Apply color map (blue = low, red = high)
    cv::Mat heatmap;
    cv::applyColorMap(normalized, heatmap, cv::COLORMAP_JET);

    // Set zero-count pixels to black
    for (int y = 0; y < data_.scattering_count.rows; ++y) {
        const int32_t* count_row = data_.scattering_count.ptr<int32_t>(y);
        cv::Vec3b* heat_row = heatmap.ptr<cv::Vec3b>(y);

        for (int x = 0; x < data_.scattering_count.cols; ++x) {
            if (count_row[x] == 0) {
                heat_row[x] = cv::Vec3b(0, 0, 0);  // Black for no scattering
            }
        }
    }

    return heatmap;
}

void ScatteringAnalyzer::update_statistics() {
    // Calculate total scattering events across all pixels and frames
    data_.total_scattering_events = 0;
    for (int y = 0; y < data_.scattering_count.rows; ++y) {
        const int32_t* count_row = data_.scattering_count.ptr<int32_t>(y);
        for (int x = 0; x < data_.scattering_count.cols; ++x) {
            data_.total_scattering_events += count_row[x];
        }
    }

    // Calculate average
    if (data_.frames_analyzed > 0) {
        data_.average_scattering_per_frame =
            (float)data_.total_scattering_events / data_.frames_analyzed;
    }
}

void ScatteringAnalyzer::update_heatmap() {
    if (data_.scattering_count.empty()) return;

    // Simple heatmap: normalize counts to 0-255
    double max_val = static_cast<double>(data_.max_scattering_count);
    if (max_val > 0) {
        data_.scattering_count.convertTo(data_.scattering_heatmap, CV_8UC1, 255.0 / max_val);
    }
}

void ScatteringAnalyzer::detect_hot_spots() {
    if (data_.scattering_count.empty()) return;

    // Find pixel with highest scattering count
    double min_val, max_val;
    cv::Point min_loc, max_loc;
    cv::minMaxLoc(data_.scattering_count, &min_val, &max_val, &min_loc, &max_loc);

    data_.max_scattering_count = static_cast<int>(max_val);
    data_.hot_spot_location = max_loc;
}
