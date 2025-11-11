/**
 * @file viewer_panel.cpp
 * @brief Implementation of ViewerPanel class
 */

#include "ui/viewer_panel.h"
#include "app_config.h"
#include "imgui.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <opencv2/imgproc.hpp>

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
}

ViewerPanel::~ViewerPanel() = default;

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

    // Noise analysis section
    ImGui::Separator();
    render_noise_analysis(camera_frame);

    // Handle dialogs
    handle_load_dialog();
    handle_save_dialog(camera_frame);
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
    if (ImGui::CollapsingHeader("Noise Analysis", ImGuiTreeNodeFlags_None)) {
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

} // namespace ui
