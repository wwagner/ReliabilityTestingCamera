/**
 * @file viewer_panel.cpp
 * @brief Implementation of ViewerPanel class
 */

#include "ui/viewer_panel.h"
#include "app_config.h"
#include "imgui.h"
#include "core/app_state.h"
#include "camera_manager.h"
#include <metavision/hal/facilities/i_ll_biases.h>
#include <metavision/hal/facilities/i_event_trail_filter_module.h>
#include <metavision/hal/facilities/i_erc_module.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <opencv2/imgproc.hpp>

// External global state (defined in main.cpp)
extern std::unique_ptr<core::AppState> app_state;

namespace ui {

// ============================================================================
// Constructor / Destructor
// ============================================================================

ViewerPanel::ViewerPanel(const std::string& name)
    : name_(name) {
    // Initialize noise params with defaults
    noise_params_.threshold_value = 128;
    noise_params_.min_area = 50;
    noise_params_.max_area = 2000;
    noise_params_.circularity_threshold = 0.7f;

    // Initialize event counting
    last_event_count_time_ = std::chrono::steady_clock::now();

    // Initialize event rate chart
    event_chart_ = std::make_unique<EventRateChart>();
}

ViewerPanel::~ViewerPanel() = default;

// ============================================================================
// Event Counting for Focus Adjust
// ============================================================================

void ViewerPanel::update_event_count(uint64_t event_count) {
    total_event_count_ += event_count;

    // Update the chart with cumulative count
    if (event_chart_) {
        event_chart_->update(total_event_count_);
    }

    // Calculate events per second every time this is called
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_event_count_time_).count();

    // Update rate calculation every 100ms minimum
    if (elapsed >= 100) {
        uint64_t events_since_last = total_event_count_ - last_event_count_;
        float seconds_elapsed = elapsed / 1000.0f;

        if (seconds_elapsed > 0) {
            events_per_second_ = events_since_last / seconds_elapsed;
        }

        last_event_count_ = total_event_count_;
        last_event_count_time_ = now;
    }
}

// ============================================================================
// Main Render
// ============================================================================

void ViewerPanel::render(const cv::Mat& camera_frame, GLuint camera_texture_id,
                        int camera_width, int camera_height) {
    // Display viewer name with loaded image filename if applicable
    if (!loaded_image_.empty() && mode_ == ViewerMode::LOADED_IMAGE && !last_loaded_path_.empty()) {
        // Extract filename from path
        std::filesystem::path path(last_loaded_path_);
        std::string filename = path.filename().string();
        ImGui::Text("%s - %s", name_.c_str(), filename.c_str());
    } else {
        ImGui::Text("%s", name_.c_str());
    }

    // Mode controls (dropdown, buttons)
    render_mode_controls();

    ImGui::Separator();

    // Display image
    render_image(camera_frame, camera_texture_id, camera_width, camera_height);

    // Event rate chart (only show for camera mode)
    if (mode_ == ViewerMode::ACTIVE_CAMERA && event_chart_) {
        ImGui::Separator();
        ImGui::Text("Event Rate Monitor");

        // Get available width for the chart
        float available_width = ImGui::GetContentRegionAvail().x;
        event_chart_->render(available_width, 100.0f);
    }

    // Noise analysis section
    ImGui::Separator();
    render_noise_analysis(camera_frame);

    // Filters section
    ImGui::Separator();
    render_filters();

    // Handle dialogs
    handle_load_dialog();
    handle_save_dialog(camera_frame);

    // Render focus adjust window if open
    render_focus_adjust_window();
}

// ============================================================================
// Mode Controls
// ============================================================================

void ViewerPanel::render_mode_controls() {
    // Dropdown selector for viewer mode
    ImGui::PushItemWidth(200);
    const char* mode_items[] = { "Active Camera", "Load Image...", "Save Image..." };
    int temp_index = selected_mode_index_;

    std::string combo_id = "##ViewerMode_" + name_;
    if (ImGui::Combo(combo_id.c_str(), &temp_index, mode_items, IM_ARRAYSIZE(mode_items))) {
        // Handle selection change
        if (temp_index == 1) {  // Load Image
            load_dialog_.open();
        } else if (temp_index == 2) {  // Save Image
            save_dialog_.open();
        } else if (temp_index == 0) {  // Active Camera
            mode_ = ViewerMode::ACTIVE_CAMERA;
            selected_mode_index_ = 0;
        }
    }
    ImGui::PopItemWidth();

    // Add switch button when showing loaded image
    if (mode_ == ViewerMode::LOADED_IMAGE && !loaded_image_.empty()) {
        if (ImGui::Button("Switch to Camera")) {
            mode_ = ViewerMode::ACTIVE_CAMERA;
            selected_mode_index_ = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Loaded Image")) {
            mode_ = ViewerMode::ACTIVE_CAMERA;
            loaded_image_ = cv::Mat();
            texture_.reset();
            selected_mode_index_ = 0;
        }
    }
}

// ============================================================================
// Image Display
// ============================================================================

void ViewerPanel::render_image(const cv::Mat& camera_frame, GLuint camera_tex_id,
                               int cam_width, int cam_height) {
    ImVec2 available_size = ImGui::GetContentRegionAvail();

    if (mode_ == ViewerMode::LOADED_IMAGE && !loaded_image_.empty()) {
        // Show loaded image with aspect ratio preserved
        if (texture_ && texture_->get_texture_id() > 0) {
            GLuint tex_id = texture_->get_texture_id();

            // Calculate proper size maintaining aspect ratio
            float img_width = static_cast<float>(loaded_image_.cols);
            float img_height = static_cast<float>(loaded_image_.rows);
            float aspect_ratio = img_width / img_height;

            ImVec2 img_size;
            img_size.x = available_size.x;
            img_size.y = img_size.x / aspect_ratio;

            // If height exceeds available space, scale by height instead
            if (img_size.y > available_size.y) {
                img_size.y = available_size.y;
                img_size.x = img_size.y * aspect_ratio;
            }

            ImGui::Image((void*)(intptr_t)tex_id, img_size);
        } else {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Preparing texture...");
        }
    } else if (mode_ == ViewerMode::ACTIVE_CAMERA) {
        // Show live camera feed with aspect ratio preserved
        if (camera_tex_id > 0 && cam_width > 0 && cam_height > 0) {
            // Calculate proper size maintaining aspect ratio
            float img_width = static_cast<float>(cam_width);
            float img_height = static_cast<float>(cam_height);
            float aspect_ratio = img_width / img_height;

            ImVec2 img_size;
            img_size.x = available_size.x;
            img_size.y = img_size.x / aspect_ratio;

            // If height exceeds available space, scale by height instead
            if (img_size.y > available_size.y) {
                img_size.y = available_size.y;
                img_size.x = img_size.y * aspect_ratio;
            }

            ImGui::Image((void*)(intptr_t)camera_tex_id, img_size);
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No camera feed");
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select an option above");
    }
}

// ============================================================================
// Noise Analysis
// ============================================================================

void ViewerPanel::render_noise_analysis(const cv::Mat& camera_frame) {
    if (ImGui::CollapsingHeader("Image Analysis", ImGuiTreeNodeFlags_None)) {
        // Get the current image to analyze
        cv::Mat current_image;
        std::string image_source;
        if (mode_ == ViewerMode::LOADED_IMAGE && !loaded_image_.empty()) {
            current_image = loaded_image_;
            image_source = "Loaded image file";
        } else if (mode_ == ViewerMode::ACTIVE_CAMERA && !camera_frame.empty()) {
            current_image = camera_frame;
            image_source = "Camera feed (Bit 0 OR Bit 7)";
        }

        bool has_image = !current_image.empty();

        // Show what image source will be analyzed
        if (has_image) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Analyzing: %s", image_source.c_str());
            ImGui::Spacing();
        }

        // Detection parameters
        if (ImGui::TreeNode("Detection Parameters")) {
            ImGui::SliderInt("Threshold", &noise_params_.threshold_value, 0, 255);
            ImGui::SetItemTooltip("Binary threshold for detecting bright dots (0-255)");

            ImGui::SliderInt("Min Dot Area", &noise_params_.min_area, 1, 500);
            ImGui::SetItemTooltip("Minimum dot size in pixels");

            ImGui::SliderInt("Max Dot Area", &noise_params_.max_area, 100, 5000);
            ImGui::SetItemTooltip("Maximum dot size in pixels");

            ImGui::SliderFloat("Circularity", &noise_params_.circularity_threshold, 0.0f, 1.0f);
            ImGui::SetItemTooltip("Minimum circularity (0-1, where 1 is perfect circle)");

            ImGui::TreePop();
        }

        // Run Analysis button
        ImGui::BeginDisabled(!has_image);
        if (ImGui::Button("Run Noise Analysis", ImVec2(200, 30))) {
            if (!noise_analyzer_) {
                noise_analyzer_ = std::make_unique<NoiseAnalyzer>();
            }

            // Set the image and process
            noise_analyzer_->setImage(current_image);
            noise_results_ = noise_analyzer_->processCurrentImage(noise_params_);
            noise_analysis_complete_ = true;

            std::cout << "Noise analysis complete for " << name_ << std::endl;
            std::cout << noise_results_.toString() << std::endl;
        }
        ImGui::EndDisabled();

        if (!has_image) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "(No image)");
        }

        // Run Focus Adjust button
        ImGui::Spacing();
        auto& cam_mgr = CameraManager::instance();
        bool camera_connected = cam_mgr.is_camera_connected(0);

        ImGui::BeginDisabled(!camera_connected);
        if (ImGui::Button("Run Focus Adjust", ImVec2(200, 30))) {
            show_focus_adjust_window_ = true;
            std::cout << "Focus adjust window opened for " << name_ << std::endl;
        }
        ImGui::EndDisabled();

        if (!camera_connected) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "(Camera required)");
        }

        // Display results if analysis is complete
        if (noise_analysis_complete_) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Analysis Results:");

            ImGui::Text("Detected Dots: %d", noise_results_.num_dots_detected);

            if (ImGui::TreeNode("Signal Statistics")) {
                ImGui::Text("Mean:     %.2f", noise_results_.signal_mean);
                ImGui::Text("Std Dev:  %.2f", noise_results_.signal_std);
                ImGui::Text("Range:    [%.0f, %.0f]", noise_results_.signal_min, noise_results_.signal_max);
                ImGui::Text("Pixels:   %d", noise_results_.num_signal_pixels);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Noise Statistics")) {
                ImGui::Text("Mean:     %.2f", noise_results_.noise_mean);
                ImGui::Text("Std Dev:  %.2f", noise_results_.noise_std);
                ImGui::Text("Range:    [%.0f, %.0f]", noise_results_.noise_min, noise_results_.noise_max);
                ImGui::Text("Pixels:   %d", noise_results_.num_noise_pixels);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Quality Metrics")) {
                // Color code SNR
                if (noise_results_.snr_db > 40.0) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "SNR: %.2f dB (Excellent)", noise_results_.snr_db);
                } else if (noise_results_.snr_db > 20.0) {
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "SNR: %.2f dB (Good)", noise_results_.snr_db);
                } else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "SNR: %.2f dB (Poor)", noise_results_.snr_db);
                }

                ImGui::Text("Contrast Ratio: %.2f", noise_results_.contrast_ratio);
                ImGui::TreePop();
            }

            // Visualization options
            if (ImGui::TreeNode("Visualization")) {
                static int viz_mode = 0;
                const char* viz_items[] = { "Detected Circles", "Signal Only", "Noise Only" };
                ImGui::Combo("Visualization Mode", &viz_mode, viz_items, IM_ARRAYSIZE(viz_items));

                if (ImGui::Button("Show Visualization", ImVec2(200, 30))) {
                    if (noise_analyzer_) {
                        cv::Mat viz_image;

                        if (viz_mode == 0) {
                            viz_image = noise_analyzer_->visualizeDetection(true);
                        } else if (viz_mode == 1) {
                            viz_image = noise_analyzer_->visualizeSignal();
                        } else if (viz_mode == 2) {
                            viz_image = noise_analyzer_->visualizeNoise();
                        }

                        // Upload to texture for display
                        if (!viz_image.empty()) {
                            if (!noise_viz_texture_) {
                                noise_viz_texture_ = std::make_unique<video::TextureManager>();
                            }

                            // Convert grayscale to BGR if needed (TextureManager expects 3-channel images)
                            cv::Mat texture_image;
                            if (viz_image.channels() == 1) {
                                cv::cvtColor(viz_image, texture_image, cv::COLOR_GRAY2BGR);
                            } else {
                                texture_image = viz_image;
                            }

                            noise_viz_texture_->upload_frame(texture_image);
                        }
                    }
                }

                // Display visualization if available
                if (noise_viz_texture_ && noise_viz_texture_->get_texture_id() > 0) {
                    ImGui::Spacing();
                    ImVec2 viz_size(300, 200);  // Fixed size for visualization preview
                    GLuint viz_tex_id = noise_viz_texture_->get_texture_id();
                    ImGui::Image((void*)(intptr_t)viz_tex_id, viz_size);
                }

                ImGui::TreePop();
            }

            // Export results button
            if (ImGui::Button("Export Results", ImVec2(200, 30))) {
                // Generate filename with timestamp
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                std::tm tm_buf;
                localtime_s(&tm_buf, &time_t);

                std::ostringstream filename;
                filename << "noise_analysis_"
                         << std::put_time(&tm_buf, "%Y%m%d_%H%M%S")
                         << ".txt";

                // Write results to file
                std::ofstream file(filename.str());
                if (file.is_open()) {
                    file << noise_results_.toString();
                    file.close();
                    std::cout << "Noise analysis results exported to: " << filename.str() << std::endl;
                }
            }
            ImGui::SetItemTooltip("Export analysis results to text file with timestamp");
        }
    }
}

// ============================================================================
// Filters Section
// ============================================================================

void ViewerPanel::render_filters() {
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_None)) {
        auto& cam_mgr = CameraManager::instance();
        bool camera_connected = cam_mgr.is_camera_connected(0);
        auto& config = AppConfig::instance();

        if (!camera_connected) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Camera not connected");
            ImGui::Text("Connect camera to adjust filters");
            return;
        }

        // Analog Bias Filters
        if (ImGui::TreeNode("Analog Bias Filters")) {
            ImGui::TextWrapped("Adjust analog bias settings to control event detection sensitivity and filtering.");
            ImGui::Spacing();

            // bias_diff_on
            int bias_diff_on = config.camera_settings().bias_diff_on;
            if (ImGui::SliderInt("ON Event Threshold", &bias_diff_on, -85, 140)) {
                config.camera_settings().bias_diff_on = bias_diff_on;
            }
            ImGui::SetItemTooltip("Controls when brightness increase triggers an event\nLower = more ON events, Higher = fewer ON events");

            // bias_diff_off
            int bias_diff_off = config.camera_settings().bias_diff_off;
            if (ImGui::SliderInt("OFF Event Threshold", &bias_diff_off, -35, 190)) {
                config.camera_settings().bias_diff_off = bias_diff_off;
            }
            ImGui::SetItemTooltip("Controls when brightness decrease triggers an event\nLower = more OFF events, Higher = fewer OFF events");

            // bias_hpf
            int bias_hpf = config.camera_settings().bias_hpf;
            if (ImGui::SliderInt("High-Pass Filter", &bias_hpf, 0, 120)) {
                config.camera_settings().bias_hpf = bias_hpf;
            }
            ImGui::SetItemTooltip("Removes DC component from signal\nHigher = stronger filtering, reduces background noise");

            // bias_refr
            int bias_refr = config.camera_settings().bias_refr;
            if (ImGui::SliderInt("Refractory Period", &bias_refr, -20, 235)) {
                config.camera_settings().bias_refr = bias_refr;
            }
            ImGui::SetItemTooltip("Prevents rapid re-triggering of same pixel\nHigher = longer dead time, reduces noise but may miss rapid changes");

            ImGui::Spacing();
            if (ImGui::Button("Apply Bias Changes", ImVec2(200, 30))) {
                // Apply biases to camera
                try {
                    auto& camera = cam_mgr.get_camera(0).camera;
                    if (camera) {
                        auto* ll_biases = camera->get_device().get_facility<Metavision::I_LL_Biases>();
                        if (ll_biases) {
                            ll_biases->set("bias_diff_on", config.camera_settings().bias_diff_on);
                            ll_biases->set("bias_diff_off", config.camera_settings().bias_diff_off);
                            ll_biases->set("bias_hpf", config.camera_settings().bias_hpf);
                            ll_biases->set("bias_refr", config.camera_settings().bias_refr);
                            std::cout << "Applied bias settings to camera" << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error applying biases: " << e.what() << std::endl;
                }
            }
            ImGui::SetItemTooltip("Apply current bias settings to the camera hardware");

            ImGui::TreePop();
        }

        // Trail Filter
        if (ImGui::TreeNode("Trail Filter")) {
            ImGui::TextWrapped("Filter noise from event bursts and rapid flickering.");
            ImGui::Spacing();

            bool trail_enabled = config.camera_settings().trail_filter_enabled;
            if (ImGui::Checkbox("Enable Trail Filter", &trail_enabled)) {
                config.camera_settings().trail_filter_enabled = trail_enabled;
            }

            int trail_type = config.camera_settings().trail_filter_type;
            const char* trail_type_items[] = { "TRAIL", "STC_CUT_TRAIL", "STC_KEEP_TRAIL" };
            if (ImGui::Combo("Filter Type", &trail_type, trail_type_items, 3)) {
                config.camera_settings().trail_filter_type = trail_type;
            }
            ImGui::SetItemTooltip("TRAIL: Basic filtering\nSTC_CUT_TRAIL: Cut trailing events\nSTC_KEEP_TRAIL: Keep stable events (recommended)");

            int trail_threshold = config.camera_settings().trail_filter_threshold;
            if (ImGui::SliderInt("Threshold (us)", &trail_threshold, 1000, 100000)) {
                config.camera_settings().trail_filter_threshold = trail_threshold;
            }
            ImGui::SetItemTooltip("Events older than this threshold are filtered (in microseconds)");

            ImGui::Spacing();
            if (ImGui::Button("Apply Trail Filter", ImVec2(200, 30))) {
                // Apply trail filter to camera
                try {
                    auto& camera = cam_mgr.get_camera(0).camera;
                    if (camera) {
                        auto* trail_filter = camera->get_device().get_facility<Metavision::I_EventTrailFilterModule>();
                        if (trail_filter) {
                            trail_filter->enable(config.camera_settings().trail_filter_enabled);
                            if (config.camera_settings().trail_filter_enabled) {
                                using FilterType = Metavision::I_EventTrailFilterModule::Type;
                                FilterType type = static_cast<FilterType>(config.camera_settings().trail_filter_type);
                                trail_filter->set_type(type);
                                trail_filter->set_threshold(config.camera_settings().trail_filter_threshold);
                            }
                            std::cout << "Applied trail filter settings to camera" << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error applying trail filter: " << e.what() << std::endl;
                }
            }
            ImGui::SetItemTooltip("Apply current trail filter settings to the camera hardware");

            ImGui::TreePop();
        }

        // Chart Settings
        if (ImGui::TreeNode("Chart Settings")) {
            ImGui::TextWrapped("Configure the event rate chart display settings.");
            ImGui::Spacing();

            // Only show if we have a chart
            if (event_chart_) {
                auto& settings = event_chart_->get_mutable_settings();

                // Time window slider
                ImGui::Text("Time Window:");
                float time_window = settings.time_window;
                if (ImGui::SliderFloat("##TimeWindow", &time_window, 10.0f, 600.0f, "%.0f seconds")) {
                    settings.time_window = time_window;
                }
                ImGui::SetItemTooltip("How many seconds of history to display (10-600 seconds)");

                ImGui::Spacing();

                // Autoscale checkbox
                bool autoscale = settings.autoscale;
                if (ImGui::Checkbox("Enable Autoscale", &autoscale)) {
                    settings.autoscale = autoscale;
                }
                ImGui::SetItemTooltip("Automatically adjust Y-axis scale based on data");

                ImGui::Spacing();

                // Min/Max rate inputs
                ImGui::Text("Y-Axis Scale (events/second):");

                // Convert to kev/s for display
                float min_kev = settings.min_rate / 1000.0f;
                float max_kev = settings.max_rate / 1000.0f;

                ImGui::PushItemWidth(100);
                ImGui::Text("Minimum:");
                ImGui::SameLine();
                if (ImGui::InputFloat("##MinRate", &min_kev, 1.0f, 10.0f, "%.1f kev/s")) {
                    settings.min_rate = min_kev * 1000.0f;
                    // Ensure min is less than max
                    if (settings.min_rate >= settings.max_rate) {
                        settings.min_rate = settings.max_rate - 1000.0f;
                    }
                    if (settings.min_rate < 100.0f) {
                        settings.min_rate = 100.0f;  // Minimum 0.1 kev/s
                    }
                }
                ImGui::SetItemTooltip("Minimum Y-axis scale (0.1-9999 kev/s)");

                ImGui::Text("Maximum:");
                ImGui::SameLine();
                if (ImGui::InputFloat("##MaxRate", &max_kev, 10.0f, 100.0f, "%.1f kev/s")) {
                    settings.max_rate = max_kev * 1000.0f;
                    // Ensure max is greater than min
                    if (settings.max_rate <= settings.min_rate) {
                        settings.max_rate = settings.min_rate + 1000.0f;
                    }
                    if (settings.max_rate > 10000000.0f) {
                        settings.max_rate = 10000000.0f;  // Maximum 10000 kev/s
                    }
                }
                ImGui::SetItemTooltip("Maximum Y-axis scale when autoscale is off (1-10000 kev/s)");
                ImGui::PopItemWidth();

                ImGui::Spacing();

                // Reset button
                if (ImGui::Button("Reset to Defaults", ImVec2(150, 0))) {
                    settings.time_window = 60.0f;
                    settings.min_rate = 1000.0f;
                    settings.max_rate = 1000000.0f;
                    settings.autoscale = true;
                }
                ImGui::SetItemTooltip("Reset all chart settings to default values");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Chart not available");
            }

            ImGui::TreePop();
        }
    }
}

// ============================================================================
// Dialog Handlers
// ============================================================================

void ViewerPanel::handle_load_dialog() {
    std::string dialog_id = get_dialog_id("Load Image");
    LoadResult result;

    if (ImageDialog::show_load_dialog(dialog_id, load_dialog_, result)) {
        // Image was successfully loaded this frame
        loaded_image_ = result.image.clone();
        loaded_metadata_ = result.metadata;
        last_loaded_path_ = result.filepath;
        mode_ = ViewerMode::LOADED_IMAGE;

        std::cout << "Image data stored in " << name_ << " state" << std::endl;

        // Create texture manager if not exists
        if (!texture_) {
            std::cout << "Creating new texture manager..." << std::endl;
            texture_ = std::make_unique<video::TextureManager>();
        }

        // Upload loaded image to texture
        std::cout << "Uploading image to GPU texture..." << std::endl;
        try {
            // Convert grayscale to BGR if needed (TextureManager expects 3-channel images)
            cv::Mat texture_image;
            if (loaded_image_.channels() == 1) {
                std::cout << "Converting grayscale to BGR for texture..." << std::endl;
                cv::cvtColor(loaded_image_, texture_image, cv::COLOR_GRAY2BGR);
            } else {
                texture_image = loaded_image_;
            }

            texture_->upload_frame(texture_image);
            std::cout << "Texture upload successful" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "ERROR uploading texture: " << e.what() << std::endl;
        }

        std::cout << "Image loaded to " << name_ << ": " << result.filepath << std::endl;
        std::cout << "  Resolution: " << result.metadata.image_width << "x" << result.metadata.image_height << std::endl;
    }
}

void ViewerPanel::handle_save_dialog(const cv::Mat& camera_frame) {
    // Determine what image to save
    cv::Mat image_to_save;
    if (mode_ == ViewerMode::LOADED_IMAGE && !loaded_image_.empty()) {
        image_to_save = loaded_image_;
    } else if (mode_ == ViewerMode::ACTIVE_CAMERA && !camera_frame.empty()) {
        image_to_save = camera_frame;
    }

    std::string dialog_id = get_dialog_id("Save Image");
    std::string saved_path;

    if (ImageDialog::show_save_dialog(dialog_id, save_dialog_, image_to_save, saved_path)) {
        // Image was successfully saved this frame
        std::cout << "Image saved from " << name_ << ": " << saved_path << std::endl;
    }
}

// ============================================================================
// Focus Adjust Window
// ============================================================================

void ViewerPanel::render_focus_adjust_window() {
    if (!show_focus_adjust_window_) {
        return;
    }

    // Create unique window ID for this viewer
    std::string window_id = "Camera Status##" + name_;

    ImGui::SetNextWindowPos(ImVec2(600, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(window_id.c_str(), &show_focus_adjust_window_)) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Focus Adjust - Camera Status");
        ImGui::Separator();
        ImGui::Spacing();

        // Get camera status
        auto& cam_mgr = CameraManager::instance();
        if (cam_mgr.is_camera_connected(0)) {
            // Get current event count from camera manager
            uint64_t current_count = cam_mgr.get_event_count();

            // Calculate rate based on count difference
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_event_count_time_).count();

            // Update rate calculation every 100ms
            if (elapsed >= 100) {
                uint64_t events_since_last = current_count - last_event_count_;
                float seconds_elapsed = elapsed / 1000.0f;

                if (seconds_elapsed > 0) {
                    events_per_second_ = events_since_last / seconds_elapsed;
                }

                last_event_count_ = current_count;
                last_event_count_time_ = now;
            }

            // Display the event rate
            ImGui::Text("Event Rate:");
            ImGui::SameLine(150);
            if (events_per_second_ >= 1000000.0f) {
                // Display in M events/sec for very large numbers
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "%.2f M ev/s", events_per_second_ / 1000000.0f);
            } else if (events_per_second_ >= 1000.0f) {
                // Display in K events/sec for large numbers
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "%.1f K ev/s", events_per_second_ / 1000.0f);
            } else if (events_per_second_ > 0.0f) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "%.0f ev/s", events_per_second_);
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Initializing...");
            }

            // Show total event count as well
            ImGui::Spacing();
            ImGui::Text("Total Events:");
            ImGui::SameLine(150);
            if (current_count >= 1000000) {
                ImGui::Text("%.2f M", current_count / 1000000.0);
            } else if (current_count >= 1000) {
                ImGui::Text("%.1f K", current_count / 1000.0);
            } else {
                ImGui::Text("%llu", current_count);
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Camera not connected");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Close button
        if (ImGui::Button("Close", ImVec2(120, 30))) {
            show_focus_adjust_window_ = false;
        }
    }
    ImGui::End();
}

} // namespace ui
