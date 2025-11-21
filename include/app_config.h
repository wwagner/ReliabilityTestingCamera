#pragma once

#include <string>

/**
 * AppConfig - Application configuration for reliability testing
 *
 * Simplified configuration for single camera reliability testing.
 * Configuration is loaded from event_config.ini in the working directory.
 */
class AppConfig {
public:
    // Camera hardware settings
    struct CameraSettings {
        // Analog bias settings
        int bias_diff = 0;          // Event detection threshold (-25 to 23)
        int bias_diff_on = 0;       // ON event threshold (-85 to 140)
        int bias_diff_off = 0;      // OFF event threshold (-35 to 190)
        int bias_fo = 0;            // Photoreceptor follower (-35 to 55)
        int bias_hpf = 0;           // High-pass filter (0 to 120)
        int bias_refr = 0;          // Refractory period (-20 to 235)

        // Frame generation
        int accumulation_time_us = 1000;  // Event accumulation period in microseconds (100-100000 μs)

        // Binary image mode settings (reliability testing)
        int binary_bit_1 = 5;       // First bit position (0-7) for binary image extraction
        int binary_bit_2 = 6;       // Second bit position (0-7) for binary image extraction

        // Trail Filter settings
        bool trail_filter_enabled = true;   // Enable trail filter by default
        int trail_filter_type = 2;          // Filter type: 0=TRAIL, 1=STC_CUT_TRAIL, 2=STC_KEEP_TRAIL
        int trail_filter_threshold = 1000;  // Threshold in microseconds

        // File I/O
        std::string capture_directory = "";  // Directory for saving captured frames (defaults to application directory)
    };

    // Runtime settings
    struct RuntimeSettings {
        bool debug_mode = false;            // Enable debug output
    };

    // Singleton access
    static AppConfig& instance();

    // Delete copy/move constructors and assignment operators
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;
    AppConfig(AppConfig&&) = delete;
    AppConfig& operator=(AppConfig&&) = delete;

    /**
     * Load configuration from event_config.ini
     * @return true if loaded successfully, false otherwise
     */
    bool load();

    /**
     * Load configuration from specified file
     * @param filename Path to config file
     * @return true if loaded successfully, false otherwise
     */
    bool load(const std::string& filename);

    /**
     * Save configuration to event_config.ini
     * @return true if saved successfully, false otherwise
     */
    bool save();

    /**
     * Save configuration to specified file
     * @param filename Path to config file
     * @return true if saved successfully, false otherwise
     */
    bool save(const std::string& filename);

    // Accessors
    CameraSettings& camera_settings() { return camera_settings_; }
    const CameraSettings& camera_settings() const { return camera_settings_; }

    RuntimeSettings& runtime_settings() { return runtime_settings_; }
    const RuntimeSettings& runtime_settings() const { return runtime_settings_; }

private:
    AppConfig() = default;
    ~AppConfig() = default;

    CameraSettings camera_settings_;
    RuntimeSettings runtime_settings_;
};
