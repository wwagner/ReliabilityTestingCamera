/**
 * @file noise_analyzer.cpp
 * @brief Implementation of Image Noise Analyzer
 */

#include "noise_analyzer.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ============================================================================
// NoiseAnalysisResults Implementation
// ============================================================================

std::string NoiseAnalysisResults::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "Noise Analysis Results\n";
    oss << "=====================\n";
    oss << "Detected dots: " << num_dots_detected << "\n";
    oss << "\nSignal Statistics:\n";
    oss << "  Mean: " << signal_mean << "\n";
    oss << "  Std Dev: " << signal_std << "\n";
    oss << "  Range: [" << signal_min << ", " << signal_max << "]\n";
    oss << "  Pixels: " << num_signal_pixels << "\n";
    oss << "\nNoise Statistics:\n";
    oss << "  Mean: " << noise_mean << "\n";
    oss << "  Std Dev: " << noise_std << "\n";
    oss << "  Range: [" << noise_min << ", " << noise_max << "]\n";
    oss << "  Pixels: " << num_noise_pixels << "\n";
    oss << "\nQuality Metrics:\n";
    oss << "  SNR: " << snr_db << " dB\n";
    oss << "  Contrast Ratio: " << contrast_ratio << "\n";
    return oss.str();
}

// ============================================================================
// NoiseAnalyzer Implementation
// ============================================================================

NoiseAnalyzer::NoiseAnalyzer() {
}

NoiseAnalyzer::~NoiseAnalyzer() {
}

bool NoiseAnalyzer::loadImage(const std::string& image_path) {
    // Load image
    cv::Mat img = cv::imread(image_path, cv::IMREAD_GRAYSCALE);

    if (img.empty()) {
        return false;
    }

    m_image = img;

    // Clear previous analysis results
    m_signal_mask = cv::Mat();
    m_noise_mask = cv::Mat();
    m_detected_circles.clear();

    return true;
}

void NoiseAnalyzer::setImage(const cv::Mat& image) {
    // Convert to grayscale if needed
    if (image.channels() == 3) {
        cv::cvtColor(image, m_image, cv::COLOR_BGR2GRAY);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, m_image, cv::COLOR_BGRA2GRAY);
    } else {
        m_image = image.clone();
    }

    // Clear previous analysis results
    m_signal_mask = cv::Mat();
    m_noise_mask = cv::Mat();
    m_detected_circles.clear();
}

int NoiseAnalyzer::detectDotsThreshold(const DotDetectionParams& params) {
    if (m_image.empty()) {
        return 0;
    }

    // Step 1: Apply binary threshold
    cv::Mat binary;
    cv::threshold(m_image, binary, params.threshold_value, 255, cv::THRESH_BINARY);

    // Step 2: Morphological operations to clean up
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_RECT,
        cv::Size(params.morph_kernel_size, params.morph_kernel_size)
    );
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);

    // Step 3: Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Step 4: Filter contours and extract circles
    m_detected_circles.clear();

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);

        // Filter by area
        if (area < params.min_area || area > params.max_area) {
            continue;
        }

        // Check circularity
        double perimeter = cv::arcLength(contour, true);
        if (perimeter <= 0) {
            continue;
        }

        float circularity = static_cast<float>(4.0 * CV_PI * area / (perimeter * perimeter));

        if (circularity > params.circularity_threshold) {
            // Get enclosing circle
            cv::Point2f center;
            float radius;
            cv::minEnclosingCircle(contour, center, radius);
            m_detected_circles.push_back(cv::Vec3f(center.x, center.y, radius));
        }
    }

    return static_cast<int>(m_detected_circles.size());
}

bool NoiseAnalyzer::createMasks(float dilation_factor) {
    if (m_image.empty()) {
        return false;
    }

    if (m_detected_circles.empty()) {
        return false;
    }

    // Create blank signal mask
    m_signal_mask = cv::Mat::zeros(m_image.size(), CV_8U);

    // Fill in detected circles
    for (const auto& circle : m_detected_circles) {
        cv::Point center(static_cast<int>(circle[0]), static_cast<int>(circle[1]));
        int radius = static_cast<int>(circle[2] * dilation_factor);
        cv::circle(m_signal_mask, center, radius, cv::Scalar(255), -1);
    }

    // Noise mask is inverse of signal mask
    m_noise_mask = cv::Mat::zeros(m_image.size(), CV_8U);
    cv::bitwise_not(m_signal_mask, m_noise_mask);

    return true;
}

void NoiseAnalyzer::calculateRegionStats(const cv::Mat& image,
                                        const cv::Mat& mask,
                                        double& mean,
                                        double& std,
                                        double& min_val,
                                        double& max_val,
                                        int& num_pixels) const {
    // Extract pixels using mask
    std::vector<double> pixels;
    pixels.reserve(cv::countNonZero(mask));

    for (int y = 0; y < image.rows; ++y) {
        const uint8_t* img_row = image.ptr<uint8_t>(y);
        const uint8_t* mask_row = mask.ptr<uint8_t>(y);

        for (int x = 0; x < image.cols; ++x) {
            if (mask_row[x] > 0) {
                pixels.push_back(static_cast<double>(img_row[x]));
            }
        }
    }

    num_pixels = static_cast<int>(pixels.size());

    if (num_pixels == 0) {
        mean = std = min_val = max_val = 0.0;
        return;
    }

    // Calculate mean
    double sum = 0.0;
    min_val = pixels[0];
    max_val = pixels[0];

    for (double val : pixels) {
        sum += val;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }
    mean = sum / num_pixels;

    // Calculate standard deviation
    double variance = 0.0;
    for (double val : pixels) {
        double diff = val - mean;
        variance += diff * diff;
    }
    std = std::sqrt(variance / num_pixels);
}

NoiseAnalysisResults NoiseAnalyzer::analyzeNoise() {
    NoiseAnalysisResults results;

    if (m_image.empty() || m_signal_mask.empty() || m_noise_mask.empty()) {
        return results;
    }

    results.num_dots_detected = static_cast<int>(m_detected_circles.size());
    results.detected_circles = m_detected_circles;

    // Calculate signal statistics
    calculateRegionStats(m_image, m_signal_mask,
                        results.signal_mean, results.signal_std,
                        results.signal_min, results.signal_max,
                        results.num_signal_pixels);

    // Calculate noise statistics
    calculateRegionStats(m_image, m_noise_mask,
                        results.noise_mean, results.noise_std,
                        results.noise_min, results.noise_max,
                        results.num_noise_pixels);

    // Calculate SNR in dB
    if (results.noise_std > 0.0) {
        double signal_power = results.signal_mean - results.noise_mean;
        results.snr_db = 20.0 * std::log10(signal_power / results.noise_std);
    } else {
        results.snr_db = std::numeric_limits<double>::infinity();
    }

    // Calculate contrast ratio
    if (results.noise_mean > 0.0) {
        results.contrast_ratio = results.signal_mean / results.noise_mean;
    } else {
        results.contrast_ratio = std::numeric_limits<double>::infinity();
    }

    // Store masks in results
    results.signal_mask = m_signal_mask.clone();
    results.noise_mask = m_noise_mask.clone();

    return results;
}

NoiseAnalysisResults NoiseAnalyzer::processImage(const std::string& image_path,
                                                const DotDetectionParams& params) {
    NoiseAnalysisResults results;

    if (!loadImage(image_path)) {
        return results;
    }

    return processCurrentImage(params);
}

NoiseAnalysisResults NoiseAnalyzer::processCurrentImage(const DotDetectionParams& params) {
    NoiseAnalysisResults results;

    if (m_image.empty()) {
        return results;
    }

    // Detect dots
    detectDotsThreshold(params);

    // Create masks
    if (!createMasks()) {
        return results;
    }

    // Analyze noise
    results = analyzeNoise();

    return results;
}

cv::Mat NoiseAnalyzer::visualizeDetection(bool show_circles) const {
    if (m_image.empty()) {
        return cv::Mat();
    }

    // Convert grayscale to color for visualization
    cv::Mat vis;
    cv::cvtColor(m_image, vis, cv::COLOR_GRAY2BGR);

    if (show_circles) {
        // Draw detected circles
        for (const auto& circle : m_detected_circles) {
            cv::Point center(static_cast<int>(circle[0]), static_cast<int>(circle[1]));
            int radius = static_cast<int>(circle[2]);

            // Draw circle outline in green
            cv::circle(vis, center, radius, cv::Scalar(0, 255, 0), 2);

            // Draw center point in red
            cv::circle(vis, center, 2, cv::Scalar(0, 0, 255), -1);
        }
    }

    return vis;
}

cv::Mat NoiseAnalyzer::visualizeSignal() const {
    if (m_image.empty() || m_signal_mask.empty()) {
        return cv::Mat();
    }

    cv::Mat signal_only = cv::Mat::zeros(m_image.size(), m_image.type());
    m_image.copyTo(signal_only, m_signal_mask);

    return signal_only;
}

cv::Mat NoiseAnalyzer::visualizeNoise() const {
    if (m_image.empty() || m_noise_mask.empty()) {
        return cv::Mat();
    }

    cv::Mat noise_only = cv::Mat::zeros(m_image.size(), m_image.type());
    m_image.copyTo(noise_only, m_noise_mask);

    return noise_only;
}
