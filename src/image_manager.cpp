#include "image_manager.h"
#include "app_config.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::string ImageManager::generate_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::tm tm;
    #ifdef _WIN32
    localtime_s(&tm, &time_t);
    #else
    localtime_r(&time_t, &tm);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H-%M-%S");
    return oss.str();
}

int64_t ImageManager::get_unix_timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return ms.count();
}

ImageManager::ImageMetadata ImageManager::create_metadata(const cv::Mat& image, const std::string& comment) {
    ImageMetadata metadata;

    // Timestamp
    metadata.timestamp = generate_timestamp();
    metadata.unix_timestamp_ms = get_unix_timestamp_ms();

    // Get configuration from AppConfig
    auto& config = AppConfig::instance();
    metadata.binary_bit_1 = config.camera_settings().binary_bit_1;
    metadata.binary_bit_2 = config.camera_settings().binary_bit_2;
    metadata.accumulation_time_us = config.camera_settings().accumulation_time_us;

    // Camera biases
    metadata.bias_diff = config.camera_settings().bias_diff;
    metadata.bias_diff_on = config.camera_settings().bias_diff_on;
    metadata.bias_diff_off = config.camera_settings().bias_diff_off;
    metadata.bias_fo = config.camera_settings().bias_fo;
    metadata.bias_hpf = config.camera_settings().bias_hpf;
    metadata.bias_refr = config.camera_settings().bias_refr;

    // Image statistics
    metadata.image_width = image.cols;
    metadata.image_height = image.rows;
    metadata.active_pixels = cv::countNonZero(image);
    metadata.pixel_density = (float)metadata.active_pixels / (image.cols * image.rows) * 100.0f;

    // User comment
    metadata.comment = comment;

    return metadata;
}

std::string ImageManager::save_image(
    const cv::Mat& image,
    const ImageMetadata& metadata,
    const std::string& directory,
    const std::string& base_filename
) {
    try {
        // Create directory if it doesn't exist
        fs::path dir_path(directory);
        if (!fs::exists(dir_path)) {
            fs::create_directories(dir_path);
            std::cout << "Created directory: " << directory << std::endl;
        }

        // Generate filename with timestamp
        std::string filename = metadata.timestamp + "_" + base_filename;
        fs::path image_path = dir_path / (filename + ".png");
        fs::path metadata_path = dir_path / (filename + ".json");

        // Save image as PNG
        std::vector<int> compression_params;
        compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        compression_params.push_back(3);  // Compression level 0-9

        if (!cv::imwrite(image_path.string(), image, compression_params)) {
            std::cerr << "Failed to save image: " << image_path << std::endl;
            return "";
        }

        std::cout << "Image saved: " << image_path << std::endl;

        // Save metadata as JSON
        if (!save_metadata_json(metadata_path.string(), metadata)) {
            std::cerr << "Warning: Failed to save metadata" << std::endl;
            // Continue anyway - image is saved
        }

        std::cout << "Metadata saved: " << metadata_path << std::endl;

        return image_path.string();

    } catch (const std::exception& e) {
        std::cerr << "Error saving image: " << e.what() << std::endl;
        return "";
    }
}

bool ImageManager::save_metadata_json(const std::string& filepath, const ImageMetadata& metadata) {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        // Write JSON manually (simple format)
        file << "{\n";
        file << "  \"timestamp\": \"" << metadata.timestamp << "\",\n";
        file << "  \"unix_timestamp_ms\": " << metadata.unix_timestamp_ms << ",\n";
        file << "  \"camera_config\": {\n";
        file << "    \"binary_bit_1\": " << metadata.binary_bit_1 << ",\n";
        file << "    \"binary_bit_2\": " << metadata.binary_bit_2 << ",\n";
        file << "    \"accumulation_time_us\": " << metadata.accumulation_time_us << "\n";
        file << "  },\n";
        file << "  \"camera_biases\": {\n";
        file << "    \"bias_diff\": " << metadata.bias_diff << ",\n";
        file << "    \"bias_diff_on\": " << metadata.bias_diff_on << ",\n";
        file << "    \"bias_diff_off\": " << metadata.bias_diff_off << ",\n";
        file << "    \"bias_fo\": " << metadata.bias_fo << ",\n";
        file << "    \"bias_hpf\": " << metadata.bias_hpf << ",\n";
        file << "    \"bias_refr\": " << metadata.bias_refr << "\n";
        file << "  },\n";
        file << "  \"image_stats\": {\n";
        file << "    \"width\": " << metadata.image_width << ",\n";
        file << "    \"height\": " << metadata.image_height << ",\n";
        file << "    \"active_pixels\": " << metadata.active_pixels << ",\n";
        file << "    \"pixel_density\": " << metadata.pixel_density << "\n";
        file << "  },\n";
        file << "  \"comment\": \"" << metadata.comment << "\",\n";
        file << "  \"app_version\": \"" << metadata.app_version << "\"\n";
        file << "}\n";

        file.close();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error writing JSON metadata: " << e.what() << std::endl;
        return false;
    }
}

bool ImageManager::load_image(
    const std::string& filepath,
    ImageMetadata& metadata,
    cv::Mat& image
) {
    try {
        fs::path image_path(filepath);

        // Load image
        image = cv::imread(filepath, cv::IMREAD_GRAYSCALE);
        if (image.empty()) {
            std::cerr << "Failed to load image: " << filepath << std::endl;
            return false;
        }

        std::cout << "Image loaded: " << filepath << std::endl;

        // Try to load metadata (optional - image can exist without metadata)
        fs::path metadata_path = image_path;
        metadata_path.replace_extension(".json");

        if (fs::exists(metadata_path)) {
            if (load_metadata_json(metadata_path.string(), metadata)) {
                std::cout << "Metadata loaded: " << metadata_path << std::endl;
            } else {
                std::cerr << "Warning: Could not parse metadata file" << std::endl;
                // Fill in basic info from image
                metadata.image_width = image.cols;
                metadata.image_height = image.rows;
                metadata.active_pixels = cv::countNonZero(image);
                metadata.pixel_density = (float)metadata.active_pixels / (image.cols * image.rows) * 100.0f;
            }
        } else {
            std::cout << "No metadata file found (this is OK for older images)" << std::endl;
            // Fill in basic info from image
            metadata.image_width = image.cols;
            metadata.image_height = image.rows;
            metadata.active_pixels = cv::countNonZero(image);
            metadata.pixel_density = (float)metadata.active_pixels / (image.cols * image.rows) * 100.0f;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error loading image: " << e.what() << std::endl;
        return false;
    }
}

bool ImageManager::load_metadata_json(const std::string& filepath, ImageMetadata& metadata) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        // Simple JSON parser (reads specific fields)
        std::string line;
        while (std::getline(file, line)) {
            // Remove whitespace and quotes
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Remove whitespace, quotes, and commas
            auto clean = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t\n\r\""));
                s.erase(s.find_last_not_of(" \t\n\r\",") + 1);
            };
            clean(key);
            clean(value);

            // Parse fields
            if (key == "timestamp") metadata.timestamp = value;
            else if (key == "unix_timestamp_ms") metadata.unix_timestamp_ms = std::stoll(value);
            else if (key == "binary_bit_1") metadata.binary_bit_1 = std::stoi(value);
            else if (key == "binary_bit_2") metadata.binary_bit_2 = std::stoi(value);
            else if (key == "accumulation_time_us") metadata.accumulation_time_us = std::stoi(value);
            else if (key == "bias_diff") metadata.bias_diff = std::stoi(value);
            else if (key == "bias_diff_on") metadata.bias_diff_on = std::stoi(value);
            else if (key == "bias_diff_off") metadata.bias_diff_off = std::stoi(value);
            else if (key == "bias_fo") metadata.bias_fo = std::stoi(value);
            else if (key == "bias_hpf") metadata.bias_hpf = std::stoi(value);
            else if (key == "bias_refr") metadata.bias_refr = std::stoi(value);
            else if (key == "width") metadata.image_width = std::stoi(value);
            else if (key == "height") metadata.image_height = std::stoi(value);
            else if (key == "active_pixels") metadata.active_pixels = std::stoi(value);
            else if (key == "pixel_density") metadata.pixel_density = std::stof(value);
            else if (key == "comment") metadata.comment = value;
            else if (key == "app_version") metadata.app_version = value;
        }

        file.close();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON metadata: " << e.what() << std::endl;
        return false;
    }
}
