/**
 * @file noise_analyzer.h
 * @brief Image Noise Analysis for Dot Pattern Images
 *
 * Analyzes images containing circular white dots (signal) on dark background (noise).
 * Separates signal from noise and calculates statistics including SNR.
 */

#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Results from noise analysis
 */
struct NoiseAnalysisResults {
    // Detection results
    int num_dots_detected = 0;
    std::vector<cv::Vec3f> detected_circles;  // (x, y, radius)

    // Signal statistics
    double signal_mean = 0.0;
    double signal_std = 0.0;
    double signal_min = 0.0;
    double signal_max = 0.0;
    int num_signal_pixels = 0;

    // Noise statistics
    double noise_mean = 0.0;
    double noise_std = 0.0;
    double noise_min = 0.0;
    double noise_max = 0.0;
    int num_noise_pixels = 0;

    // Quality metrics
    double snr_db = 0.0;           // Signal-to-Noise Ratio in decibels
    double contrast_ratio = 0.0;   // Signal mean / Noise mean

    // Masks
    cv::Mat signal_mask;  // Boolean mask for signal regions
    cv::Mat noise_mask;   // Boolean mask for noise regions

    /**
     * @brief Convert results to string for display
     */
    std::string toString() const;
};

/**
 * @brief Parameters for dot detection
 */
struct DotDetectionParams {
    // Threshold-based detection
    int threshold_value = 128;        // Binary threshold (0-255)
    int min_area = 50;                // Minimum dot area in pixels
    int max_area = 2000;              // Maximum dot area in pixels
    float circularity_threshold = 0.7f;  // Minimum circularity (0-1)

    // Morphological operations
    int morph_kernel_size = 3;        // Kernel size for morphological ops
};

/**
 * @brief Image Noise Analyzer
 *
 * Detects circular dots in images and separates signal from noise for analysis.
 */
class NoiseAnalyzer {
public:
    /**
     * @brief Construct a new Noise Analyzer
     */
    NoiseAnalyzer();

    /**
     * @brief Destroy the Noise Analyzer
     */
    ~NoiseAnalyzer();

    /**
     * @brief Load an image from file
     *
     * @param image_path Path to the image file
     * @return true if successful, false otherwise
     */
    bool loadImage(const std::string& image_path);

    /**
     * @brief Set the image directly
     *
     * @param image Image to analyze (will be converted to grayscale if needed)
     */
    void setImage(const cv::Mat& image);

    /**
     * @brief Detect dots using threshold-based contour detection
     *
     * This method uses binary thresholding and contour analysis to detect circular dots.
     * It's more robust than Hough circles for varying dot intensities.
     *
     * @param params Detection parameters
     * @return Number of dots detected
     */
    int detectDotsThreshold(const DotDetectionParams& params = DotDetectionParams());

    /**
     * @brief Create signal and noise masks from detected circles
     *
     * @param dilation_factor Factor to scale circle radius (>1 to include edges)
     * @return true if successful, false if no circles detected
     */
    bool createMasks(float dilation_factor = 1.0f);

    /**
     * @brief Analyze noise statistics
     *
     * @return Analysis results
     */
    NoiseAnalysisResults analyzeNoise();

    /**
     * @brief Complete processing pipeline
     *
     * Performs dot detection, mask creation, and noise analysis in one call.
     *
     * @param image_path Path to image file
     * @param params Detection parameters
     * @return Analysis results
     */
    NoiseAnalysisResults processImage(const std::string& image_path,
                                     const DotDetectionParams& params = DotDetectionParams());

    /**
     * @brief Process the currently loaded image
     *
     * @param params Detection parameters
     * @return Analysis results
     */
    NoiseAnalysisResults processCurrentImage(const DotDetectionParams& params = DotDetectionParams());

    /**
     * @brief Get the loaded image
     */
    const cv::Mat& getImage() const { return m_image; }

    /**
     * @brief Get detected circles
     */
    const std::vector<cv::Vec3f>& getDetectedCircles() const { return m_detected_circles; }

    /**
     * @brief Get signal mask
     */
    const cv::Mat& getSignalMask() const { return m_signal_mask; }

    /**
     * @brief Get noise mask
     */
    const cv::Mat& getNoiseMask() const { return m_noise_mask; }

    /**
     * @brief Create visualization of detection results
     *
     * @param show_circles If true, draw detected circles
     * @return RGB image with visualization overlay
     */
    cv::Mat visualizeDetection(bool show_circles = true) const;

    /**
     * @brief Create visualization of signal regions
     *
     * @return Grayscale image showing only signal pixels
     */
    cv::Mat visualizeSignal() const;

    /**
     * @brief Create visualization of noise regions
     *
     * @return Grayscale image showing only noise pixels
     */
    cv::Mat visualizeNoise() const;

private:
    cv::Mat m_image;                          // Grayscale image
    cv::Mat m_signal_mask;                    // Boolean mask for signal
    cv::Mat m_noise_mask;                     // Boolean mask for noise
    std::vector<cv::Vec3f> m_detected_circles;  // Detected circles (x, y, radius)

    /**
     * @brief Calculate statistics for masked region
     */
    void calculateRegionStats(const cv::Mat& image,
                             const cv::Mat& mask,
                             double& mean,
                             double& std,
                             double& min_val,
                             double& max_val,
                             int& num_pixels) const;
};
