#include "app_config.h"
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <stdlib.h>  // For _fullpath
#include <direct.h>  // For _MAX_PATH
#else
#include <limits.h>  // For PATH_MAX
#include <stdlib.h>  // For realpath
#endif

AppConfig& AppConfig::instance() {
    static AppConfig instance;
    return instance;
}

bool AppConfig::load() {
    return load("event_config.ini");
}

bool AppConfig::load(const std::string& filename) {
    // Get absolute path for debugging
    #ifdef _WIN32
    char absolute_path[_MAX_PATH];
    _fullpath(absolute_path, filename.c_str(), _MAX_PATH);
    std::cout << "Loading config from: " << absolute_path << std::endl;
    #else
    char absolute_path[PATH_MAX];
    realpath(filename.c_str(), absolute_path);
    std::cout << "Loading config from: " << absolute_path << std::endl;
    #endif

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Config file not found: " << filename << " (using defaults)" << std::endl;
        return false;
    }

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        // Skip comments
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        // Check for section header
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            section = line.substr(1, line.length() - 2);
            continue;
        }

        // Parse key=value
        size_t equals = line.find('=');
        if (equals == std::string::npos) continue;

        std::string key = line.substr(0, equals);
        std::string value = line.substr(equals + 1);

        // Trim key and value
        key = key.substr(0, key.find_last_not_of(" \t") + 1);
        value = value.substr(value.find_first_not_of(" \t"));

        // Parse settings based on section
        if (section == "Camera") {
            if (key == "bias_diff") camera_settings_.bias_diff = std::stoi(value);
            else if (key == "bias_diff_on") camera_settings_.bias_diff_on = std::stoi(value);
            else if (key == "bias_diff_off") camera_settings_.bias_diff_off = std::stoi(value);
            else if (key == "bias_fo") camera_settings_.bias_fo = std::stoi(value);
            else if (key == "bias_hpf") camera_settings_.bias_hpf = std::stoi(value);
            else if (key == "bias_refr") camera_settings_.bias_refr = std::stoi(value);
            else if (key == "accumulation_time_us") camera_settings_.accumulation_time_us = std::stoi(value);
            else if (key == "binary_bit_1") camera_settings_.binary_bit_1 = std::stoi(value);
            else if (key == "binary_bit_2") camera_settings_.binary_bit_2 = std::stoi(value);
            else if (key == "trail_filter_enabled") camera_settings_.trail_filter_enabled = (value == "true" || value == "1");
            else if (key == "trail_filter_type") camera_settings_.trail_filter_type = std::stoi(value);
            else if (key == "trail_filter_threshold") camera_settings_.trail_filter_threshold = std::stoi(value);
            else if (key == "capture_directory") camera_settings_.capture_directory = value;
        }
        else if (section == "Runtime") {
            if (key == "debug_mode") runtime_settings_.debug_mode = (value == "true" || value == "1");
        }
    }

    std::cout << "Configuration loaded successfully" << std::endl;
    std::cout << "  Accumulation time: " << camera_settings_.accumulation_time_us << " μs" << std::endl;
    std::cout << "  Binary bits: " << camera_settings_.binary_bit_1 << ", " << camera_settings_.binary_bit_2 << std::endl;
    std::cout << "  Trail filter: " << (camera_settings_.trail_filter_enabled ? "enabled" : "disabled") << std::endl;

    return true;
}

bool AppConfig::save() {
    return save("event_config.ini");
}

bool AppConfig::save(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file for writing: " << filename << std::endl;
        return false;
    }

    // Write camera settings
    file << "[Camera]\n";
    file << "bias_diff = " << camera_settings_.bias_diff << "\n";
    file << "bias_diff_on = " << camera_settings_.bias_diff_on << "\n";
    file << "bias_diff_off = " << camera_settings_.bias_diff_off << "\n";
    file << "bias_fo = " << camera_settings_.bias_fo << "\n";
    file << "bias_hpf = " << camera_settings_.bias_hpf << "\n";
    file << "bias_refr = " << camera_settings_.bias_refr << "\n";
    file << "accumulation_time_us = " << camera_settings_.accumulation_time_us << "\n";
    file << "binary_bit_1 = " << camera_settings_.binary_bit_1 << "\n";
    file << "binary_bit_2 = " << camera_settings_.binary_bit_2 << "\n";
    file << "trail_filter_enabled = " << (camera_settings_.trail_filter_enabled ? "true" : "false") << "\n";
    file << "trail_filter_type = " << camera_settings_.trail_filter_type << "\n";
    file << "trail_filter_threshold = " << camera_settings_.trail_filter_threshold << "\n";
    if (!camera_settings_.capture_directory.empty()) {
        file << "capture_directory = " << camera_settings_.capture_directory << "\n";
    }
    file << "\n";

    // Write runtime settings
    file << "[Runtime]\n";
    file << "debug_mode = " << (runtime_settings_.debug_mode ? "true" : "false") << "\n";

    std::cout << "Configuration saved to: " << filename << std::endl;
    return true;
}
