#pragma once

#include <opencv2/core.hpp>
#include <string>

/**
 * ScatteringAnalyzer - Detects and tracks noise pixels in event camera data
 *
 * "Scattering" refers to pixels that appear in the live camera feed but were
 * NOT present in the reference/baseline image. These represent noise or
 * unreliable pixels that should be monitored for reliability testing.
 */
class ScatteringAnalyzer {
public:
    struct ScatteringData {
        // Current frame analysis
        cv::Mat scattering_mask;           // Binary mask: 255 = scattering pixel
        int current_scattering_pixels;     // Count of scattering pixels in current frame
        float current_scattering_percentage;

        // Temporal tracking
        cv::Mat scattering_count;          // Accumulator: tracks how many times each pixel scattered
        cv::Mat scattering_heatmap;        // Visualization (0-255 intensity)
        int frames_analyzed;               // Number of frames processed

        // Hot spot detection
        int max_scattering_count;          // Highest count for any single pixel
        cv::Point hot_spot_location;       // Location of most frequently scattering pixel

        // Overall statistics
        int total_scattering_events;       // Sum of all scattering across all frames
        float average_scattering_per_frame;
    };

    ScatteringAnalyzer();
    ~ScatteringAnalyzer() = default;

    /**
     * Start new scattering analysis with a reference image
     * @param reference_image The baseline image (binary, grayscale)
     * @return true if analysis started successfully
     */
    bool start_analysis(const cv::Mat& reference_image);

    /**
     * Analyze current frame for scattering pixels
     * @param live_image Current camera frame (binary, grayscale)
     * @return true if analysis successful
     */
    bool analyze_frame(const cv::Mat& live_image);

    /**
     * Stop current analysis and reset
     */
    void stop_analysis();

    /**
     * Check if analysis is currently active
     */
    bool is_analyzing() const { return analyzing_; }

    /**
     * Get current scattering data
     */
    const ScatteringData& get_data() const { return data_; }

    /**
     * Create visualization image with scattering pixels highlighted
     * @param live_image Current camera frame
     * @param highlight_color Color for scattering pixels (BGR)
     * @return Color image with scattering highlighted
     */
    cv::Mat create_scattering_visualization(
        const cv::Mat& live_image,
        const cv::Scalar& highlight_color = cv::Scalar(255, 0, 255)  // Magenta
    ) const;

    /**
     * Create heatmap visualization showing scattering frequency
     * @return Color-coded heatmap (blue=low, red=high)
     */
    cv::Mat create_heatmap_visualization() const;

    /**
     * Reset temporal tracking (keeps reference, clears counts)
     */
    void reset_temporal_data();

private:
    bool analyzing_;
    cv::Mat reference_image_;
    ScatteringData data_;

    void update_statistics();
    void update_heatmap();
    void detect_hot_spots();
};
