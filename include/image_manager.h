#pragma once

#include <string>
#include <opencv2/core.hpp>
#include <chrono>

/**
 * ImageManager - Handles saving and loading images with metadata
 *
 * Reliability testing application stores:
 * - PNG image (binary processed frame)
 * - JSON metadata (timestamp, camera settings, user comments)
 */
class ImageManager {
public:
    /**
     * Metadata structure for saved images
     */
    struct ImageMetadata {
        // Timestamp information
        std::string timestamp;              // ISO 8601 format: "2025-11-10T14:30:45"
        int64_t unix_timestamp_ms;          // Milliseconds since epoch

        // Camera configuration (at time of capture)
        int binary_bit_1;                   // First bit position (0-7)
        int binary_bit_2;                   // Second bit position (0-7)
        int accumulation_time_us;           // Frame accumulation in microseconds

        // Camera biases (for reference)
        int bias_diff;
        int bias_diff_on;
        int bias_diff_off;
        int bias_fo;
        int bias_hpf;
        int bias_refr;

        // Image statistics
        int image_width;
        int image_height;
        int active_pixels;                  // Number of white pixels
        float pixel_density;                // Percentage of active pixels

        // User annotation
        std::string comment;                // User comment/notes

        // Application info
        std::string app_version = "1.0.0";
    };

    /**
     * Save image with metadata
     * @param image Image to save (binary processed frame)
     * @param metadata Metadata structure
     * @param directory Directory to save to
     * @param base_filename Base filename (timestamp will be prepended)
     * @return Full path of saved file, or empty string on error
     */
    static std::string save_image(
        const cv::Mat& image,
        const ImageMetadata& metadata,
        const std::string& directory,
        const std::string& base_filename = "reliability_test"
    );

    /**
     * Load image with metadata
     * @param filepath Full path to image file
     * @param metadata Output metadata structure
     * @param image Output loaded image
     * @return true if successful
     */
    static bool load_image(
        const std::string& filepath,
        ImageMetadata& metadata,
        cv::Mat& image
    );

    /**
     * Generate current timestamp string
     * @return ISO 8601 formatted timestamp
     */
    static std::string generate_timestamp();

    /**
     * Get current Unix timestamp in milliseconds
     * @return Milliseconds since epoch
     */
    static int64_t get_unix_timestamp_ms();

    /**
     * Create metadata from current application state
     * @param image Current camera image
     * @param comment User comment
     * @return Populated metadata structure
     */
    static ImageMetadata create_metadata(const cv::Mat& image, const std::string& comment);

    /**
     * Save metadata as JSON
     * @param filepath Path to JSON file
     * @param metadata Metadata to save
     * @return true if successful
     */
    static bool save_metadata_json(const std::string& filepath, const ImageMetadata& metadata);

private:

    /**
     * Load metadata from JSON
     * @param filepath Path to JSON file
     * @param metadata Output metadata structure
     * @return true if successful
     */
    static bool load_metadata_json(const std::string& filepath, ImageMetadata& metadata);
};
