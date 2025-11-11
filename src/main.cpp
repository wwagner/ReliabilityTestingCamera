/**
 * Reliability Testing Camera Application
 *
 * Single event camera viewer for reliability testing and noise evaluation.
 * Displays live camera feed and allows comparison with saved images.
 */

#include <iostream>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <fstream>

// OpenGL/GLFW/ImGui
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// OpenCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

// Metavision SDK
#include <metavision/sdk/driver/camera.h>
#include <metavision/sdk/core/algorithms/periodic_frame_generation_algorithm.h>
#include <metavision/hal/facilities/i_ll_biases.h>
#include <metavision/hal/facilities/i_roi.h>
#include <metavision/hal/facilities/i_erc_module.h>
#include <metavision/hal/facilities/i_monitoring.h>
#include <metavision/hal/facilities/i_antiflicker_module.h>
#include <metavision/hal/facilities/i_event_trail_filter_module.h>

// Local headers
#include "camera_manager.h"
#include "video/simd_utils.h"
#include "video/gpu_compute.h"
#include "app_config.h"
#include "image_manager.h"
#include "scattering_analyzer.h"
#include "noise_analyzer.h"
#include "ui/image_dialog.h"
#include "ui/viewer_panel.h"

// Camera features
#include "camera/features/erc_feature.h"
#include "camera/features/antiflicker_feature.h"
#include "camera/features/trail_filter_feature.h"
#include "camera/features/roi_feature.h"
#include "camera/features/monitoring_feature.h"

// Application state module
#include "core/app_state.h"

// UI modules
#include "ui/settings_panel.h"
#include "camera/bias_manager.h"

// Force usage of discrete GPU on laptops
#ifdef _WIN32
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// ============================================================================
// Global State
// ============================================================================

// Application state - SINGLE CAMERA ONLY
std::unique_ptr<core::AppState> app_state;

// Binary bit storage for single camera
struct BinaryBitStorage {
    cv::Mat bit1;  // First extracted bit
    cv::Mat bit2;  // Second extracted bit
    cv::Mat combined;  // Combined binary image
};
static BinaryBitStorage camera_bits;

// Loaded image for comparison (Phase 3)
cv::Mat loaded_image;
ImageManager::ImageMetadata loaded_metadata;
std::unique_ptr<video::TextureManager> loaded_image_texture;

// Image save/load state
std::string last_saved_path;
std::string last_loaded_path;

// Viewer panels (Phase 6 - Refactored in Phase 1.2)
// ViewerMode enum and ViewerState struct moved to ui::ViewerPanel class
std::unique_ptr<ui::ViewerPanel> left_viewer;
std::unique_ptr<ui::ViewerPanel> right_viewer;

// Comparison mode (Phase 4)
enum class ComparisonMode {
    NONE,           // No comparison - show live and loaded separately
    OVERLAY,        // Blend live and loaded images
    DIFFERENCE,     // Show pixel differences
    SIDEBYSIDE      // Side-by-side view (default) - renamed to avoid Windows macro conflict
};

struct ComparisonState {
    ComparisonMode mode = ComparisonMode::NONE;
    float overlay_alpha = 0.5f;  // Blend factor for overlay mode (0.0 = live only, 1.0 = loaded only)
    cv::Mat comparison_image;    // Result of comparison operation
    std::unique_ptr<video::TextureManager> comparison_texture;

    // Statistics
    int pixels_in_live_only = 0;
    int pixels_in_loaded_only = 0;
    int pixels_in_both = 0;
    int pixels_different = 0;
    float difference_percentage = 0.0f;
} comparison_state;

// Scattering analysis (Phase 5)
enum class ScatteringViewMode {
    NONE,           // No scattering visualization
    HIGHLIGHT,      // Highlight scattering pixels on live image
    HEATMAP         // Show scattering frequency heatmap
};

struct ScatteringState {
    std::unique_ptr<ScatteringAnalyzer> analyzer;
    ScatteringViewMode view_mode = ScatteringViewMode::NONE;
    cv::Mat visualization;
    std::unique_ptr<video::TextureManager> scattering_texture;
    bool continuous_analysis = false;  // Auto-analyze each frame
} scattering_state;

// UI state (Phase 6)
static bool show_help_window = false;

// ============================================================================
// Binary Image Processing
// ============================================================================

/**
 * Create lookup table for bit extraction
 * Maps pixels with specific bit set to 255, all others to 0
 */
static cv::Mat create_bit_extraction_lut(int bit_position) {
    cv::Mat lut(1, 256, CV_8U);
    uint8_t mask = (1 << bit_position);
    for (int i = 0; i < 256; i++) {
        lut.at<uchar>(i) = (i & mask) ? 255 : 0;
    }
    return lut;
}

/**
 * Extract and process binary image from camera frame
 * Uses bit positions specified in INI file
 */
void process_camera_frame(const cv::Mat& frame) {
    if (frame.empty()) return;
    if (!app_state) {
        std::cerr << "Error: app_state is null in process_camera_frame" << std::endl;
        return;
    }

    // Extract grayscale channel
    cv::Mat gray;
    if (frame.channels() == 3) {
        gray = cv::Mat(frame.size(), CV_8UC1);
        cv::extractChannel(frame, gray, 0);
    } else {
        gray = frame;
    }

    // Get bit positions from INI configuration
    int bit1_pos = static_cast<int>(app_state->display_settings().get_binary_stream_mode());
    int bit2_pos = static_cast<int>(app_state->display_settings().get_binary_stream_mode_2());

    // Extract both bits using LUT
    cv::Mat lut1 = create_bit_extraction_lut(bit1_pos);
    cv::Mat lut2 = create_bit_extraction_lut(bit2_pos);

    camera_bits.bit1 = cv::Mat(gray.size(), CV_8UC1);
    camera_bits.bit2 = cv::Mat(gray.size(), CV_8UC1);

    cv::LUT(gray, lut1, camera_bits.bit1);
    cv::LUT(gray, lut2, camera_bits.bit2);

    // Combine bits using OR operation
    cv::bitwise_or(camera_bits.bit1, camera_bits.bit2, camera_bits.combined);

    // Store in frame buffer for display
    app_state->frame_buffer(0).store_frame(camera_bits.combined);

    // Continuous scattering analysis (Phase 5)
    if (scattering_state.continuous_analysis &&
        scattering_state.analyzer &&
        scattering_state.analyzer->is_analyzing()) {
        scattering_state.analyzer->analyze_frame(camera_bits.combined);

        // Update visualization based on view mode
        if (scattering_state.view_mode == ScatteringViewMode::HIGHLIGHT) {
            scattering_state.visualization = scattering_state.analyzer->create_scattering_visualization(
                camera_bits.combined,
                cv::Scalar(255, 0, 255)  // Magenta
            );
        } else if (scattering_state.view_mode == ScatteringViewMode::HEATMAP) {
            scattering_state.visualization = scattering_state.analyzer->create_heatmap_visualization();
        }

        // Upload to texture for display
        if (!scattering_state.visualization.empty()) {
            if (!scattering_state.scattering_texture) {
                scattering_state.scattering_texture = std::make_unique<video::TextureManager>();
            }
            scattering_state.scattering_texture->upload_frame(scattering_state.visualization);
        }
    }
}

// ============================================================================
// Image Comparison Functions
// ============================================================================

/**
 * Calculate comparison statistics between live and loaded images
 */
void calculate_comparison_statistics(const cv::Mat& live_img, const cv::Mat& loaded_img) {
    if (live_img.empty() || loaded_img.empty()) return;
    if (live_img.size() != loaded_img.size()) {
        std::cerr << "Images must be same size for comparison" << std::endl;
        return;
    }

    comparison_state.pixels_in_live_only = 0;
    comparison_state.pixels_in_loaded_only = 0;
    comparison_state.pixels_in_both = 0;
    comparison_state.pixels_different = 0;

    int total_pixels = live_img.rows * live_img.cols;

    for (int y = 0; y < live_img.rows; ++y) {
        const uint8_t* live_row = live_img.ptr<uint8_t>(y);
        const uint8_t* loaded_row = loaded_img.ptr<uint8_t>(y);

        for (int x = 0; x < live_img.cols; ++x) {
            bool live_active = live_row[x] > 0;
            bool loaded_active = loaded_row[x] > 0;

            if (live_active && loaded_active) {
                comparison_state.pixels_in_both++;
            } else if (live_active && !loaded_active) {
                comparison_state.pixels_in_live_only++;
            } else if (!live_active && loaded_active) {
                comparison_state.pixels_in_loaded_only++;
            }

            if (live_active != loaded_active) {
                comparison_state.pixels_different++;
            }
        }
    }

    comparison_state.difference_percentage =
        (float)comparison_state.pixels_different / total_pixels * 100.0f;
}

/**
 * Create overlay image (blend live and loaded images)
 */
cv::Mat create_overlay_image(const cv::Mat& live_img, const cv::Mat& loaded_img, float alpha) {
    if (live_img.empty() || loaded_img.empty()) return cv::Mat();
    if (live_img.size() != loaded_img.size()) {
        std::cerr << "Images must be same size for overlay" << std::endl;
        return cv::Mat();
    }

    cv::Mat overlay;

    // Convert to 3-channel for color overlay
    cv::Mat live_color, loaded_color;
    cv::cvtColor(live_img, live_color, cv::COLOR_GRAY2BGR);
    cv::cvtColor(loaded_img, loaded_color, cv::COLOR_GRAY2BGR);

    // Color code: live = green, loaded = red, both = yellow
    cv::Mat result = cv::Mat::zeros(live_img.size(), CV_8UC3);

    for (int y = 0; y < live_img.rows; ++y) {
        const uint8_t* live_row = live_img.ptr<uint8_t>(y);
        const uint8_t* loaded_row = loaded_img.ptr<uint8_t>(y);
        cv::Vec3b* result_row = result.ptr<cv::Vec3b>(y);

        for (int x = 0; x < live_img.cols; ++x) {
            bool live_active = live_row[x] > 0;
            bool loaded_active = loaded_row[x] > 0;

            if (live_active && loaded_active) {
                // Both active: Yellow
                result_row[x] = cv::Vec3b(0, 255, 255);  // BGR: Yellow
            } else if (live_active) {
                // Live only: Green
                result_row[x] = cv::Vec3b(0, 255, 0);  // BGR: Green
            } else if (loaded_active) {
                // Loaded only: Red
                result_row[x] = cv::Vec3b(0, 0, 255);  // BGR: Red
            }
        }
    }

    return result;
}

/**
 * Create difference image (highlight differences)
 */
cv::Mat create_difference_image(const cv::Mat& live_img, const cv::Mat& loaded_img) {
    if (live_img.empty() || loaded_img.empty()) return cv::Mat();
    if (live_img.size() != loaded_img.size()) {
        std::cerr << "Images must be same size for difference" << std::endl;
        return cv::Mat();
    }

    cv::Mat diff;
    cv::absdiff(live_img, loaded_img, diff);

    // Convert to color for better visualization
    cv::Mat diff_color;
    cv::cvtColor(diff, diff_color, cv::COLOR_GRAY2BGR);

    // Make differences more visible (white = different, black = same)
    for (int y = 0; y < diff_color.rows; ++y) {
        cv::Vec3b* row = diff_color.ptr<cv::Vec3b>(y);
        for (int x = 0; x < diff_color.cols; ++x) {
            if (row[x][0] > 0) {
                row[x] = cv::Vec3b(255, 255, 255);  // White for differences
            }
        }
    }

    return diff_color;
}

/**
 * Perform image comparison based on current mode
 */
void perform_comparison() {
    if (camera_bits.combined.empty() || loaded_image.empty()) {
        std::cerr << "Both live and loaded images required for comparison" << std::endl;
        return;
    }

    // Check size compatibility
    if (camera_bits.combined.size() != loaded_image.size()) {
        std::cerr << "Image size mismatch: Live("
                  << camera_bits.combined.cols << "x" << camera_bits.combined.rows
                  << ") vs Loaded("
                  << loaded_image.cols << "x" << loaded_image.rows << ")" << std::endl;
        return;
    }

    // Calculate statistics
    calculate_comparison_statistics(camera_bits.combined, loaded_image);

    // Create comparison image based on mode
    switch (comparison_state.mode) {
        case ComparisonMode::OVERLAY:
            comparison_state.comparison_image = create_overlay_image(
                camera_bits.combined, loaded_image, comparison_state.overlay_alpha);
            break;

        case ComparisonMode::DIFFERENCE:
            comparison_state.comparison_image = create_difference_image(
                camera_bits.combined, loaded_image);
            break;

        default:
            comparison_state.comparison_image = cv::Mat();
            break;
    }

    // Upload to texture if image was created
    if (!comparison_state.comparison_image.empty()) {
        if (!comparison_state.comparison_texture) {
            comparison_state.comparison_texture = std::make_unique<video::TextureManager>();
        }
        comparison_state.comparison_texture->upload_frame(comparison_state.comparison_image);
    }

    std::cout << "Comparison complete:" << std::endl;
    std::cout << "  Pixels in live only: " << comparison_state.pixels_in_live_only << std::endl;
    std::cout << "  Pixels in loaded only: " << comparison_state.pixels_in_loaded_only << std::endl;
    std::cout << "  Pixels in both: " << comparison_state.pixels_in_both << std::endl;
    std::cout << "  Difference: " << comparison_state.difference_percentage << "%" << std::endl;
}

// ============================================================================
// Camera Management
// ============================================================================

/**
 * Initialize camera (but don't start yet - following EventCamera pattern)
 */
bool initialize_camera() {
    try {
        auto& config = AppConfig::instance();
        auto& cam_mgr = CameraManager::instance();

        // Initialize camera with accumulation time from config (but don't start)
        if (!cam_mgr.initialize_single_camera(config.camera_settings().accumulation_time_us)) {
            std::cerr << "Failed to initialize camera" << std::endl;
            return false;
        }

        std::cout << "Camera initialized (callbacks will be set up after UI initialization)" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Camera initialization error: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// UI Rendering
// ============================================================================

/**
 * Render main control panel with action buttons
 */
void render_control_panel() {
    ImGui::Begin("Reliability Testing Controls");

    // Help button (top right)
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 80);
    if (ImGui::Button("Help [F1]", ImVec2(70, 25))) {
        show_help_window = !show_help_window;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Show/hide help window.\nKeyboard: F1");
    }

    ImGui::Text("Camera Status:");
    ImGui::SameLine();
    auto& cam_mgr = CameraManager::instance();
    if (cam_mgr.is_camera_connected(0)) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disconnected");
    }

    ImGui::Separator();

    // Save Image (enabled only if camera has an image)
    bool has_camera_image = !camera_bits.combined.empty();
    ImGui::BeginDisabled(!has_camera_image);
    if (ImGui::Button("Save Image", ImVec2(200, 30))) {
        ImGui::OpenPopup("Save Image");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Save current camera image with timestamp and metadata.\nRequires active camera feed.");
    }
    ImGui::EndDisabled();
    if (!has_camera_image) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "(No camera image)");
    }

    // Load Image (always enabled)
    if (ImGui::Button("Load Image", ImVec2(200, 30))) {
        ImGui::OpenPopup("Load Image");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Load a previously saved image for comparison.\nSupports PNG files with or without metadata.");
    }

    // Compare Images (enabled only if loaded image exists)
    ImGui::BeginDisabled(loaded_image.empty() || camera_bits.combined.empty());
    if (ImGui::Button("Compare Images", ImVec2(200, 30))) {
        // Toggle comparison mode
        if (comparison_state.mode == ComparisonMode::NONE) {
            comparison_state.mode = ComparisonMode::OVERLAY;
        }
        perform_comparison();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Compare live camera with loaded image.\nShows differences using color-coded overlay.\nRequires both camera feed and loaded image.");
    }
    ImGui::EndDisabled();
    if (loaded_image.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "(Load image first)");
    }

    // Comparison mode selection (only show when comparison is active)
    if (comparison_state.mode != ComparisonMode::NONE) {
        ImGui::Separator();
        ImGui::Text("Comparison Mode:");

        if (ImGui::RadioButton("Overlay", comparison_state.mode == ComparisonMode::OVERLAY)) {
            comparison_state.mode = ComparisonMode::OVERLAY;
            perform_comparison();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Color-coded overlay:\nYellow = Both images\nGreen = Live only\nRed = Loaded only");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Difference", comparison_state.mode == ComparisonMode::DIFFERENCE)) {
            comparison_state.mode = ComparisonMode::DIFFERENCE;
            perform_comparison();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Binary difference:\nWhite = Different pixels\nBlack = Identical pixels");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Side-by-Side", comparison_state.mode == ComparisonMode::SIDEBYSIDE)) {
            comparison_state.mode = ComparisonMode::SIDEBYSIDE;
            comparison_state.comparison_image = cv::Mat();  // Clear comparison image
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show original images:\nLeft = Live camera\nRight = Loaded reference");
        }

        // Stop comparison button
        if (ImGui::Button("Stop Comparison", ImVec2(200, 30))) {
            comparison_state.mode = ComparisonMode::NONE;
            comparison_state.comparison_image = cv::Mat();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Exit comparison mode.\nReturns to normal view.");
        }
    }

    // Phase 5: Calculate Scattering
    ImGui::Separator();
    ImGui::Text("Scattering Analysis:");

    bool has_reference = !loaded_image.empty();
    bool can_start_scattering = has_reference && !camera_bits.combined.empty();

    if (!has_reference) {
        ImGui::BeginDisabled();
    }

    if (!scattering_state.analyzer || !scattering_state.analyzer->is_analyzing()) {
        // Start scattering analysis
        if (ImGui::Button("Start Scattering Analysis", ImVec2(200, 30))) {
            if (can_start_scattering) {
                if (!scattering_state.analyzer) {
                    scattering_state.analyzer = std::make_unique<ScatteringAnalyzer>();
                }
                if (scattering_state.analyzer->start_analysis(loaded_image)) {
                    scattering_state.view_mode = ScatteringViewMode::HIGHLIGHT;
                    scattering_state.continuous_analysis = true;
                    std::cout << "Scattering analysis started" << std::endl;
                }
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Detect noise pixels that appear in live camera\nbut NOT in reference image.\nTracks scattering over time.\nRequires loaded reference image.");
        }
        if (!has_reference) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "(Load reference image first)");
        }
    } else {
        // Stop scattering analysis
        if (ImGui::Button("Stop Scattering Analysis", ImVec2(200, 30))) {
            if (scattering_state.analyzer) {
                scattering_state.analyzer->stop_analysis();
                scattering_state.view_mode = ScatteringViewMode::NONE;
                scattering_state.continuous_analysis = false;
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Stop scattering detection.\nPreserves accumulated data.");
        }

        // View mode selection
        ImGui::Text("View Mode:");
        if (ImGui::RadioButton("Highlight", scattering_state.view_mode == ScatteringViewMode::HIGHLIGHT)) {
            scattering_state.view_mode = ScatteringViewMode::HIGHLIGHT;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Show scattering pixels in magenta.\nReal-time noise detection.");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Heatmap", scattering_state.view_mode == ScatteringViewMode::HEATMAP)) {
            scattering_state.view_mode = ScatteringViewMode::HEATMAP;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Frequency heatmap:\nBlue = Low scattering\nRed = High scattering\nShows cumulative pattern.");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("None##scatter", scattering_state.view_mode == ScatteringViewMode::NONE)) {
            scattering_state.view_mode = ScatteringViewMode::NONE;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Hide scattering visualization.\nStatistics continue updating.");
        }

        // Reset button
        if (ImGui::Button("Reset Temporal Data", ImVec2(200, 30))) {
            if (scattering_state.analyzer) {
                scattering_state.analyzer->reset_temporal_data();
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Clear accumulated scattering counts.\nKeeps reference image.\nUse to start fresh tracking session.");
        }
    }

    if (!has_reference) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    // Data Export Section
    ImGui::Text("Data Export:");

    // Export scattering statistics to CSV
    bool can_export_stats = scattering_state.analyzer && scattering_state.analyzer->is_analyzing();
    ImGui::BeginDisabled(!can_export_stats);
    if (ImGui::Button("Export Statistics (CSV)", ImVec2(200, 30))) {
        ImGui::OpenPopup("Export Statistics");
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Export scattering statistics to CSV file.\nIncludes temporal tracking data.\nRequires active scattering analysis.");
    }
    ImGui::EndDisabled();

    // Export heatmap image
    bool can_export_heatmap = scattering_state.analyzer &&
                              scattering_state.analyzer->is_analyzing() &&
                              scattering_state.analyzer->get_data().frames_analyzed > 0;
    ImGui::BeginDisabled(!can_export_heatmap);
    if (ImGui::Button("Export Heatmap (PNG)", ImVec2(200, 30))) {
        // Export heatmap image
        auto heatmap = scattering_state.analyzer->create_heatmap_visualization();
        if (!heatmap.empty()) {
            auto& config = AppConfig::instance();
            std::string timestamp = ImageManager::generate_timestamp();
            std::string filename = config.camera_settings().capture_directory + "\\" + timestamp + "_scattering_heatmap.png";
            if (cv::imwrite(filename, heatmap)) {
                std::cout << "Heatmap exported: " << filename << std::endl;
            }
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Export scattering heatmap as PNG image.\nShows frequency distribution.\nRequires scattering data.");
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // Display current binary bit settings (read-only from INI)
    ImGui::Text("Binary Bit Configuration:");
    if (app_state) {
        int bit1 = static_cast<int>(app_state->display_settings().get_binary_stream_mode());
        int bit2 = static_cast<int>(app_state->display_settings().get_binary_stream_mode_2());
        ImGui::Text("  Bit 1 Position: %d", bit1);
        ImGui::Text("  Bit 2 Position: %d", bit2);
    } else {
        ImGui::Text("  Initializing...");
    }
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  (Read-only from INI file)");

    ImGui::End();

    // Export Statistics popup
    if (ImGui::BeginPopupModal("Export Statistics", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export scattering statistics to CSV file");
        ImGui::Separator();

        static char csv_filename[256] = "";
        ImGui::Text("Filename (optional):");
        ImGui::InputText("##csvfilename", csv_filename, 256);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Leave blank for automatic timestamp");

        ImGui::Separator();

        if (ImGui::Button("Export", ImVec2(120, 0))) {
            if (scattering_state.analyzer && scattering_state.analyzer->is_analyzing()) {
                auto& config = AppConfig::instance();
                std::string timestamp = ImageManager::generate_timestamp();
                std::string base_name = strlen(csv_filename) > 0 ? csv_filename : ("scattering_stats_" + timestamp);
                std::string filepath = config.camera_settings().capture_directory + "\\" + base_name + ".csv";

                // Export CSV
                std::ofstream csv(filepath);
                if (csv.is_open()) {
                    const auto& data = scattering_state.analyzer->get_data();

                    // Header
                    csv << "Scattering Analysis Statistics\n";
                    csv << "Timestamp," << timestamp << "\n";
                    csv << "\n";

                    // Current frame stats
                    csv << "Current Frame Statistics\n";
                    csv << "Scattering Pixels," << data.current_scattering_pixels << "\n";
                    csv << "Scattering Percentage," << data.current_scattering_percentage << "\n";
                    csv << "\n";

                    // Temporal stats
                    csv << "Temporal Statistics\n";
                    csv << "Frames Analyzed," << data.frames_analyzed << "\n";
                    csv << "Total Scattering Events," << data.total_scattering_events << "\n";
                    csv << "Average Per Frame," << data.average_scattering_per_frame << "\n";
                    csv << "\n";

                    // Hot spot
                    csv << "Hot Spot Detection\n";
                    csv << "Location X," << data.hot_spot_location.x << "\n";
                    csv << "Location Y," << data.hot_spot_location.y << "\n";
                    csv << "Frequency," << data.max_scattering_count << "\n";

                    csv.close();
                    std::cout << "Statistics exported: " << filepath << std::endl;
                }

                csv_filename[0] = '\0';  // Clear input
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            csv_filename[0] = '\0';  // Clear input
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Save Image popup
    if (ImGui::BeginPopupModal("Save Image", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save current image with metadata");
        ImGui::Separator();

        static char comment[512] = "";
        ImGui::Text("Comment:");
        ImGui::InputTextMultiline("##Comment", comment, 512, ImVec2(400, 100));

        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            // Get current camera image
            if (!camera_bits.combined.empty()) {
                auto& config = AppConfig::instance();

                // Create metadata with user comment
                auto metadata = ImageManager::create_metadata(camera_bits.combined, std::string(comment));

                // Save image and metadata
                std::string saved_path = ImageManager::save_image(
                    camera_bits.combined,
                    metadata,
                    config.camera_settings().capture_directory,
                    "reliability_test"
                );

                if (!saved_path.empty()) {
                    last_saved_path = saved_path;
                    std::cout << "Image saved successfully: " << saved_path << std::endl;

                    // Clear comment for next save
                    comment[0] = '\0';
                } else {
                    std::cerr << "Failed to save image" << std::endl;
                }
            } else {
                std::cerr << "No image to save" << std::endl;
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Load Image popup
    if (ImGui::BeginPopupModal("Load Image", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Load image for comparison");
        ImGui::Separator();

        static char filepath[512] = "";
        ImGui::Text("Image file path:");
        ImGui::InputText("##FilePath", filepath, 512);
        ImGui::Text("(Enter full path to PNG file)");

        ImGui::Separator();

        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (strlen(filepath) > 0) {
                cv::Mat loaded_img;
                ImageManager::ImageMetadata metadata;

                if (ImageManager::load_image(filepath, metadata, loaded_img)) {
                    loaded_image = loaded_img;
                    loaded_metadata = metadata;
                    last_loaded_path = filepath;

                    // Create texture manager for loaded image if not exists
                    if (!loaded_image_texture) {
                        loaded_image_texture = std::make_unique<video::TextureManager>();
                    }

                    // Upload loaded image to texture
                    loaded_image_texture->upload_frame(loaded_image);

                    std::cout << "Image loaded successfully: " << filepath << std::endl;
                    std::cout << "  Resolution: " << metadata.image_width << "x" << metadata.image_height << std::endl;
                    std::cout << "  Active pixels: " << metadata.active_pixels << std::endl;
                    if (!metadata.comment.empty()) {
                        std::cout << "  Comment: " << metadata.comment << std::endl;
                    }

                    // Clear path for next load
                    filepath[0] = '\0';
                } else {
                    std::cerr << "Failed to load image: " << filepath << std::endl;
                }
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::Separator();
        ImGui::Text("Quick tip: Paste the path from File Explorer");

        ImGui::EndPopup();
    }
}

/**
 * Render statistics panel
 */
void render_statistics_panel() {
    ImGui::Begin("Image Statistics");

    if (!camera_bits.combined.empty()) {
        // Calculate statistics for live camera image
        cv::Scalar mean = cv::mean(camera_bits.combined);
        int nonzero = cv::countNonZero(camera_bits.combined);
        int total = camera_bits.combined.rows * camera_bits.combined.cols;
        float density = (float)nonzero / total * 100.0f;

        ImGui::Text("Live Camera:");
        ImGui::Text("  Active Pixels: %d / %d (%.2f%%)", nonzero, total, density);
        ImGui::Text("  Mean Value: %.2f", mean[0]);
        ImGui::Text("  Resolution: %d x %d", camera_bits.combined.cols, camera_bits.combined.rows);
    }

    ImGui::Separator();

    if (!loaded_image.empty()) {
        // Display statistics for loaded image
        ImGui::Text("Loaded Image:");
        ImGui::Text("  Resolution: %d x %d", loaded_metadata.image_width, loaded_metadata.image_height);
        ImGui::Text("  Active Pixels: %d / %d (%.2f%%)",
                    loaded_metadata.active_pixels,
                    loaded_metadata.image_width * loaded_metadata.image_height,
                    loaded_metadata.pixel_density);

        if (!loaded_metadata.timestamp.empty()) {
            ImGui::Text("  Captured: %s", loaded_metadata.timestamp.c_str());
        }

        if (!loaded_metadata.comment.empty()) {
            ImGui::TextWrapped("  Comment: %s", loaded_metadata.comment.c_str());
        }

        // Binary configuration when image was captured
        if (loaded_metadata.binary_bit_1 >= 0) {
            ImGui::Text("  Binary bits: %d, %d", loaded_metadata.binary_bit_1, loaded_metadata.binary_bit_2);
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No image loaded for comparison");
    }

    // Comparison statistics (Phase 4)
    if (comparison_state.mode != ComparisonMode::NONE && comparison_state.pixels_different > 0) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 1, 1), "Comparison Statistics:");

        ImGui::Text("  In both images: %d", comparison_state.pixels_in_both);
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "  Live only: %d", comparison_state.pixels_in_live_only);
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "  Loaded only: %d", comparison_state.pixels_in_loaded_only);
        ImGui::Text("  Different: %d (%.2f%%)",
                    comparison_state.pixels_different,
                    comparison_state.difference_percentage);

        // Color legend for overlay mode
        if (comparison_state.mode == ComparisonMode::OVERLAY) {
            ImGui::Separator();
            ImGui::Text("Color Legend:");
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "  Yellow = Both");
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "  Green = Live only");
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "  Red = Loaded only");
        }
    }

    // Scattering analysis statistics (Phase 5)
    if (scattering_state.analyzer && scattering_state.analyzer->is_analyzing()) {
        const auto& data = scattering_state.analyzer->get_data();

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0, 1, 1), "Scattering Analysis:");

        // Current frame statistics
        ImGui::Text("Current Frame:");
        ImGui::TextColored(ImVec4(1, 0, 1, 1), "  Scattering pixels: %d (%.2f%%)",
                          data.current_scattering_pixels,
                          data.current_scattering_percentage);

        // Temporal tracking
        if (data.frames_analyzed > 0) {
            ImGui::Separator();
            ImGui::Text("Temporal Tracking:");
            ImGui::Text("  Frames analyzed: %d", data.frames_analyzed);
            ImGui::Text("  Total scattering events: %d", data.total_scattering_events);
            ImGui::Text("  Average per frame: %.1f", data.average_scattering_per_frame);

            // Hot spot information
            if (data.max_scattering_count > 0) {
                ImGui::Separator();
                ImGui::Text("Hot Spot:");
                ImGui::Text("  Location: (%d, %d)", data.hot_spot_location.x, data.hot_spot_location.y);
                ImGui::Text("  Frequency: %d times", data.max_scattering_count);
            }
        }

        // View mode legend
        if (scattering_state.view_mode == ScatteringViewMode::HIGHLIGHT) {
            ImGui::Separator();
            ImGui::Text("Visualization:");
            ImGui::TextColored(ImVec4(1, 0, 1, 1), "  Magenta = Scattering pixels");
            ImGui::Text("  (Pixels in live but NOT in reference)");
        } else if (scattering_state.view_mode == ScatteringViewMode::HEATMAP) {
            ImGui::Separator();
            ImGui::Text("Heatmap:");
            ImGui::TextColored(ImVec4(0, 0, 1, 1), "  Blue = Low frequency");
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "  Red = High frequency");
        }
    }

    ImGui::End();
}

/**
 * Render help window with usage instructions
 */
void render_help_window() {
    if (!show_help_window) return;

    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Help - Reliability Testing Camera", &show_help_window)) {
        ImGui::TextColored(ImVec4(0, 1, 1, 1), "Reliability Testing Camera - User Guide");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Getting Started", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped(
                "This application tests event camera reliability by detecting noise pixels "
                "and comparing images over time."
            );
            ImGui::Spacing();
            ImGui::BulletText("Camera must be connected before starting");
            ImGui::BulletText("All settings are read from event_config.ini");
            ImGui::BulletText("Images are saved with timestamps and metadata");
        }

        if (ImGui::CollapsingHeader("Basic Workflow")) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "1. Save Baseline Image:");
            ImGui::Indent();
            ImGui::BulletText("Start camera and wait for stable image");
            ImGui::BulletText("Click 'Save Image' button");
            ImGui::BulletText("Add optional comment");
            ImGui::BulletText("Image saved with timestamp");
            ImGui::Unindent();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(1, 1, 0, 1), "2. Load Reference:");
            ImGui::Indent();
            ImGui::BulletText("Click 'Load Image' button");
            ImGui::BulletText("Enter or paste full path to PNG file");
            ImGui::BulletText("Image appears in right view");
            ImGui::Unindent();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(1, 1, 0, 1), "3. Compare or Analyze:");
            ImGui::Indent();
            ImGui::BulletText("Compare: Shows differences between live and loaded");
            ImGui::BulletText("Scattering: Detects noise pixels over time");
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Image Comparison")) {
            ImGui::TextWrapped("Compare live camera with saved reference image.");
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Overlay Mode:");
            ImGui::Indent();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Yellow"); ImGui::SameLine(); ImGui::Text("= Pixels in both images");
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Green"); ImGui::SameLine(); ImGui::Text("= Pixels only in live");
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Red"); ImGui::SameLine(); ImGui::Text("= Pixels only in loaded");
            ImGui::Unindent();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Difference Mode:");
            ImGui::Indent();
            ImGui::Text("White = Different pixels");
            ImGui::Text("Black = Identical pixels");
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Scattering Analysis")) {
            ImGui::TextWrapped(
                "Scattering detects noise pixels that appear in live camera but NOT "
                "in reference image. Useful for reliability testing."
            );
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(1, 0, 1, 1), "Highlight Mode:");
            ImGui::Indent();
            ImGui::BulletText("Magenta pixels = scattering (noise)");
            ImGui::BulletText("Real-time detection");
            ImGui::Unindent();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Heatmap Mode:");
            ImGui::Indent();
            ImGui::BulletText("Blue = Low frequency scattering");
            ImGui::BulletText("Red = High frequency scattering");
            ImGui::BulletText("Shows cumulative pattern");
            ImGui::Unindent();
            ImGui::Spacing();

            ImGui::TextWrapped("Use 'Reset Temporal Data' to clear counts and start fresh.");
        }

        if (ImGui::CollapsingHeader("Data Export")) {
            ImGui::BulletText("Export Statistics (CSV): Save scattering metrics");
            ImGui::BulletText("Export Heatmap (PNG): Save frequency visualization");
            ImGui::BulletText("All exports timestamped automatically");
        }

        if (ImGui::CollapsingHeader("Keyboard Shortcuts")) {
            ImGui::Text("F1"); ImGui::SameLine(100); ImGui::Text("Show/hide this help window");
            ImGui::Text("ESC"); ImGui::SameLine(100); ImGui::Text("Close application");
        }

        if (ImGui::CollapsingHeader("Troubleshooting")) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Camera not connected:");
            ImGui::Indent();
            ImGui::BulletText("Check USB connection");
            ImGui::BulletText("Restart application");
            ImGui::Unindent();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Buttons grayed out:");
            ImGui::Indent();
            ImGui::BulletText("Hover over button for tooltip");
            ImGui::BulletText("Check prerequisites (camera, loaded image, etc.)");
            ImGui::Unindent();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Reliability Testing Camera v1.0");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "For more info, see PROJECT.md and PHASE documentation");
    }
    ImGui::End();
}

/**
 * Handle load image dialog for a viewer (refactored to use ImageDialog)
 */
// Old viewer functions removed - now handled by ui::ViewerPanel class
// - handle_viewer_load_dialog()
// - handle_viewer_save_dialog()
// - render_viewer()


/**
 * Render dual camera view (left = live, right = loaded/comparison)
 */
void render_camera_views() {
    // Set default position and size for first run
    ImGui::SetNextWindowPos(ImVec2(310, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1300, 800), ImGuiCond_FirstUseEver);

    ImGui::Begin("Camera Views", nullptr, ImGuiWindowFlags_NoScrollbar);

    ImVec2 window_size = ImGui::GetContentRegionAvail();
    float view_width = (window_size.x - 20) / 2.0f;  // Two views side by side
    float view_height = window_size.y - 40;

    // Get camera texture info
    GLuint camera_tex_id = 0;
    int cam_width = 0;
    int cam_height = 0;
    if (app_state && app_state->texture_manager(0).get_texture_id() > 0) {
        camera_tex_id = app_state->texture_manager(0).get_texture_id();
        cam_width = app_state->texture_manager(0).get_width();
        cam_height = app_state->texture_manager(0).get_height();
    }

    // Left viewer
    ImGui::BeginChild("LeftViewer", ImVec2(view_width, view_height), true);
    if (left_viewer) {
        left_viewer->render(camera_bits.combined, camera_tex_id, cam_width, cam_height);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right viewer
    ImGui::BeginChild("RightViewer", ImVec2(view_width, view_height), true);
    if (right_viewer) {
        right_viewer->render(camera_bits.combined, camera_tex_id, cam_width, cam_height);
    }
    ImGui::EndChild();

    ImGui::End();
}

// ============================================================================
// OpenGL/GLFW Setup
// ============================================================================

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// ============================================================================
// Main Application Loop
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Reliability Testing Camera ===" << std::endl;
    std::cout << "Single event camera viewer for reliability testing" << std::endl;

    // Load configuration from INI file (read-only)
    auto& config = AppConfig::instance();
    if (!config.load("event_config.ini")) {
        std::cerr << "Warning: Could not load event_config.ini, using defaults" << std::endl;
    } else {
        std::cout << "Configuration loaded from event_config.ini" << std::endl;
        std::cout << "  Binary Bit 1: " << config.camera_settings().binary_bit_1 << std::endl;
        std::cout << "  Binary Bit 2: " << config.camera_settings().binary_bit_2 << std::endl;
        std::cout << "  Accumulation Time: " << config.camera_settings().accumulation_time_us << " μs" << std::endl;
    }

    // Initialize viewer panels
    left_viewer = std::make_unique<ui::ViewerPanel>("Left Viewer");
    right_viewer = std::make_unique<ui::ViewerPanel>("Right Viewer");

    // Initialize camera BEFORE UI (following EventCamera pattern)
    bool camera_connected = initialize_camera();
    if (!camera_connected) {
        std::cerr << "Warning: Camera not connected. Running in simulation mode." << std::endl;
    }

    // Initialize GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }

    // Create window
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Reliability Testing Camera", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync

    // Initialize GLEW
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize application state
    app_state = std::make_unique<core::AppState>();

    // Configure binary stream mode from INI
    app_state->display_settings().set_binary_stream_mode(
        static_cast<core::DisplaySettings::BinaryStreamMode>(config.camera_settings().binary_bit_1)
    );
    app_state->display_settings().set_binary_stream_mode_2(
        static_cast<core::DisplaySettings::BinaryStreamMode>(config.camera_settings().binary_bit_2)
    );

    // Now start the camera (callbacks + camera->start())
    if (camera_connected) {
        std::cout << "\nStarting camera..." << std::endl;

        auto& cam_mgr = CameraManager::instance();
        auto callback = [](const cv::Mat& frame, int camera_index) {
            process_camera_frame(frame);
        };

        if (!cam_mgr.start_single_camera(callback)) {
            std::cerr << "Failed to start camera" << std::endl;
            camera_connected = false;
        } else {
            std::cout << "Camera started successfully" << std::endl;
        }
    }

    // Main loop
    std::cout << "\nEntering main loop..." << std::endl;

    try {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Keyboard shortcuts (Phase 6)
            if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
                show_help_window = !show_help_window;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                glfwSetWindowShouldClose(window, true);
            }

            render_camera_views();
            // render_control_panel();  // Removed per user request
            // render_statistics_panel();  // Removed per user request
            render_help_window();

            // Viewer dialogs now handled internally by ViewerPanel::render()

            // Update texture from frame buffer
            if (camera_connected && app_state) {
                auto frame_opt = app_state->frame_buffer(0).consume_frame();
                if (frame_opt.has_value()) {
                    app_state->texture_manager(0).upload_frame(frame_opt.value());
                }
            }

            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Fatal exception in main loop: " << e.what() << std::endl;
        std::cerr << "This is likely a camera/HAL error. Try restarting the application or camera." << std::endl;
        // Continue to cleanup
    } catch (...) {
        std::cerr << "ERROR: Unknown exception in main loop (not derived from std::exception)" << std::endl;
        std::cerr << "This is likely a severe error. The application will now shutdown." << std::endl;
        // Continue to cleanup
    }

    // Cleanup
    std::cout << "Shutting down..." << std::endl;

    CameraManager::instance().shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Application closed successfully" << std::endl;
    return 0;
}

// ============================================================================
// File Dialog Implementation (after all type definitions to avoid macro conflicts)
// ============================================================================

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

// Old open_file_dialog() function removed - now handled by ui::ImageDialog class
