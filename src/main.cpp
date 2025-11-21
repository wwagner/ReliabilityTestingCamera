/**
 * Reliability Testing Camera Application
 *
 * Single event camera viewer for reliability testing and noise evaluation.
 * Displays live camera feed with integrated noise analysis and filter controls.
 */

#include <iostream>
#include <memory>
#include <filesystem>

// OpenGL/GLFW/ImGui
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// OpenCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// Metavision SDK
#include <metavision/sdk/driver/camera.h>
#include <metavision/hal/facilities/i_ll_biases.h>
#include <metavision/hal/facilities/i_erc_module.h>
#include <metavision/hal/facilities/i_antiflicker_module.h>
#include <metavision/hal/facilities/i_event_trail_filter_module.h>

// Local headers
#include "camera_manager.h"
#include "app_config.h"
#include "ui/viewer_panel.h"
#include "core/app_state.h"

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

std::unique_ptr<core::AppState> app_state;
std::unique_ptr<ui::ViewerPanel> viewer;

// Binary bit storage for camera frame
struct BinaryBitStorage {
    cv::Mat bit1;
    cv::Mat bit2;
    cv::Mat combined;
};
static BinaryBitStorage camera_bits;

// UI state
static bool show_help_window = false;

// ============================================================================
// Binary Image Processing
// ============================================================================

/**
 * Create lookup table for bit extraction
 */
static cv::Mat create_bit_extraction_lut(int bit_position) {
    cv::Mat lut(1, 256, CV_8U);
    int bit_value = 1 << bit_position;

    for (int i = 0; i < 256; ++i) {
        lut.at<uint8_t>(i) = (i & bit_value) ? 255 : 0;
    }

    return lut;
}

/**
 * Process camera frame: extract binary bits and combine
 */
void process_camera_frame(const cv::Mat& frame) {
    if (frame.empty() || !app_state) return;

    // Get bit positions from config
    int bit1_pos = static_cast<int>(app_state->display_settings().get_binary_stream_mode());
    int bit2_pos = static_cast<int>(app_state->display_settings().get_binary_stream_mode_2());

    // Event camera frames are grayscale, but may have 3 channels with duplicate data
    // Extract first channel to ensure single-channel processing
    cv::Mat gray_frame;
    if (frame.channels() == 3) {
        // Extract just the first channel (all channels should be identical for grayscale)
        cv::extractChannel(frame, gray_frame, 0);
    } else {
        gray_frame = frame;
    }

    // Extract both bits using LUT
    static cv::Mat lut1 = create_bit_extraction_lut(bit1_pos);
    static cv::Mat lut2 = create_bit_extraction_lut(bit2_pos);

    cv::LUT(gray_frame, lut1, camera_bits.bit1);
    cv::LUT(gray_frame, lut2, camera_bits.bit2);

    // Combine with OR operation (single-channel OR)
    cv::bitwise_or(camera_bits.bit1, camera_bits.bit2, camera_bits.combined);

    // Verify we have single-channel output
    static bool once = true;
    if (once) {
        std::cout << "Bit1 channels=" << camera_bits.bit1.channels() << std::endl;
        std::cout << "Bit2 channels=" << camera_bits.bit2.channels() << std::endl;
        std::cout << "Combined channels=" << camera_bits.combined.channels() << std::endl;
        once = false;
    }

    // Store in frame buffer for display (single-channel binary image)
    app_state->frame_buffer(0).store_frame(camera_bits.combined);
}

// ============================================================================
// Camera Management
// ============================================================================

/**
 * Apply initial camera settings from config (biases, filters, etc.)
 */
void apply_initial_camera_settings() {
    auto& config = AppConfig::instance();
    auto& cam_mgr = CameraManager::instance();

    if (!cam_mgr.is_camera_connected(0)) {
        std::cerr << "Cannot apply settings: camera not connected" << std::endl;
        return;
    }

    try {
        auto& camera = cam_mgr.get_camera(0).camera;
        if (!camera) return;

        std::cout << "Applying initial camera settings from config..." << std::endl;

        // Apply analog biases
        auto* ll_biases = camera->get_device().get_facility<Metavision::I_LL_Biases>();
        if (ll_biases) {
            ll_biases->set("bias_diff", config.camera_settings().bias_diff);
            ll_biases->set("bias_diff_on", config.camera_settings().bias_diff_on);
            ll_biases->set("bias_diff_off", config.camera_settings().bias_diff_off);
            ll_biases->set("bias_fo", config.camera_settings().bias_fo);
            ll_biases->set("bias_hpf", config.camera_settings().bias_hpf);
            ll_biases->set("bias_refr", config.camera_settings().bias_refr);
            std::cout << "  - Applied analog biases" << std::endl;
        }

        // Apply trail filter
        auto* trail_filter = camera->get_device().get_facility<Metavision::I_EventTrailFilterModule>();
        if (trail_filter) {
            trail_filter->enable(config.camera_settings().trail_filter_enabled);
            if (config.camera_settings().trail_filter_enabled) {
                using FilterType = Metavision::I_EventTrailFilterModule::Type;
                FilterType type = static_cast<FilterType>(config.camera_settings().trail_filter_type);
                trail_filter->set_type(type);
                trail_filter->set_threshold(config.camera_settings().trail_filter_threshold);
                std::cout << "  - Applied trail filter (enabled)" << std::endl;
            } else {
                std::cout << "  - Trail filter disabled" << std::endl;
            }
        }

        std::cout << "Initial camera settings applied successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error applying initial camera settings: " << e.what() << std::endl;
    }
}

/**
 * Initialize camera
 */
bool initialize_camera() {
    try {
        auto& config = AppConfig::instance();
        auto& cam_mgr = CameraManager::instance();

        if (!cam_mgr.initialize_single_camera(config.camera_settings().accumulation_time_us)) {
            std::cerr << "Failed to initialize camera" << std::endl;
            return false;
        }

        std::cout << "Camera initialized successfully" << std::endl;
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
 * Render simple status panel
 */
void render_status_panel() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 120), ImGuiCond_FirstUseEver);

    ImGui::Begin("Status");

    // Help button
    if (ImGui::Button("Help [F1]", ImVec2(-1, 25))) {
        show_help_window = !show_help_window;
    }

    ImGui::Separator();

    // Camera status
    ImGui::Text("Camera:");
    ImGui::SameLine(100);
    auto& cam_mgr = CameraManager::instance();
    if (cam_mgr.is_camera_connected(0)) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Disconnected");
    }

    // Binary bit configuration
    ImGui::Text("Binary Bits:");
    ImGui::SameLine(100);
    if (app_state) {
        int bit1 = static_cast<int>(app_state->display_settings().get_binary_stream_mode());
        int bit2 = static_cast<int>(app_state->display_settings().get_binary_stream_mode_2());
        ImGui::Text("%d, %d", bit1, bit2);
    }

    ImGui::End();
}

/**
 * Render camera viewer (single viewer with all controls)
 */
void render_camera_views() {
    ImGui::SetNextWindowPos(ImVec2(310, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900, 800), ImGuiCond_FirstUseEver);

    ImGui::Begin("Camera View", nullptr, ImGuiWindowFlags_NoScrollbar);

    // Get camera texture info
    GLuint camera_tex_id = 0;
    int cam_width = 0;
    int cam_height = 0;
    if (app_state && app_state->texture_manager(0).get_texture_id() > 0) {
        camera_tex_id = app_state->texture_manager(0).get_texture_id();
        cam_width = app_state->texture_manager(0).get_width();
        cam_height = app_state->texture_manager(0).get_height();
    }

    // Render viewer (includes image display, noise analysis, and filters)
    if (viewer) {
        viewer->render(camera_bits.combined, camera_tex_id, cam_width, cam_height);
    }

    ImGui::End();
}

/**
 * Render help window
 */
void render_help_window() {
    if (!show_help_window) return;

    ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Help", &show_help_window)) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Reliability Testing Camera - Help");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextWrapped(
            "This application allows you to view and analyze event camera feeds for reliability testing.\n\n"
        );

        if (ImGui::CollapsingHeader("Camera View", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::BulletText("Active Camera: View live camera feed");
            ImGui::BulletText("Load Image: Load a saved image for analysis");
            ImGui::BulletText("Save Image: Save current camera frame with metadata");
        }

        if (ImGui::CollapsingHeader("Image Analysis", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextWrapped("Analyzes images to detect circular dots (signal) and measure background noise.");
            ImGui::BulletText("Adjust detection parameters to tune sensitivity");
            ImGui::BulletText("Run analysis to get SNR, contrast ratio, and statistics");
            ImGui::BulletText("Export results to text file for reporting");
            ImGui::BulletText("Run Focus Adjust: Monitor event rate in real-time");
        }

        if (ImGui::CollapsingHeader("Camera Settings")) {
            ImGui::TextWrapped("Adjust camera biases and trail filter settings in real-time.");
            ImGui::BulletText("Analog Bias Filters: Control event detection sensitivity");
            ImGui::BulletText("ON/OFF Thresholds: Adjust brightness change sensitivity");
            ImGui::BulletText("High-Pass Filter: Remove DC noise component");
            ImGui::BulletText("Refractory Period: Prevent pixel re-triggering");
            ImGui::BulletText("Trail Filter: Filter transient events and flickering");
            ImGui::BulletText("Click 'Apply' buttons to send changes to camera");
        }

        if (ImGui::CollapsingHeader("Binary Image Mode")) {
            ImGui::TextWrapped(
                "The camera extracts specific bits from the 8-bit image for reliability testing.\n"
                "Configured in event_config.ini (binary_bit_1 and binary_bit_2).\n\n"
                "Current configuration displayed in Status panel."
            );
        }

        if (ImGui::CollapsingHeader("Keyboard Shortcuts")) {
            ImGui::BulletText("F1: Toggle this help window");
            ImGui::BulletText("ESC: Close application");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "For detailed documentation, see README.md");
    }
    ImGui::End();
}

// ============================================================================
// OpenGL/GLFW Setup
// ============================================================================

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// ============================================================================
// Main Application
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Reliability Testing Camera ===" << std::endl;
    std::cout << "Single camera viewer for event camera reliability testing" << std::endl;
    std::cout << std::endl;

    // Load configuration
    auto& config = AppConfig::instance();
    if (!config.load()) {
        std::cerr << "Warning: Failed to load config, using defaults" << std::endl;
    }

    // Display capture directory
    if (!config.camera_settings().capture_directory.empty()) {
        std::cout << "Capture directory: " << config.camera_settings().capture_directory << std::endl;
    } else {
        std::cout << "Capture directory: [Application directory]" << std::endl;
    }

    // Initialize application state
    app_state = std::make_unique<core::AppState>();

    // Configure binary stream mode from config
    app_state->display_settings().set_binary_stream_mode(
        static_cast<core::DisplaySettings::BinaryStreamMode>(config.camera_settings().binary_bit_1)
    );
    app_state->display_settings().set_binary_stream_mode_2(
        static_cast<core::DisplaySettings::BinaryStreamMode>(config.camera_settings().binary_bit_2)
    );

    // Initialize viewer panel
    viewer = std::make_unique<ui::ViewerPanel>("Camera Viewer");

    // Initialize camera
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

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Reliability Testing Camera", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return 1;
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    std::cout << "UI initialized successfully" << std::endl;

    // Start camera if connected
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
            apply_initial_camera_settings();
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

            // Update texture from frame buffer
            if (camera_connected && app_state) {
                auto frame_opt = app_state->frame_buffer(0).consume_frame();
                if (frame_opt.has_value()) {
                    app_state->texture_manager(0).upload_frame(frame_opt.value());
                }

                // Update event count for the viewer panel chart
                // The camera manager's get_event_count returns cumulative count
                // The viewer's update_event_count expects incremental count
                // So we pass the cumulative count and let the viewer handle it
                if (viewer) {
                    static uint64_t last_count = 0;
                    uint64_t current_count = CameraManager::instance().get_event_count();
                    uint64_t event_delta = current_count - last_count;
                    last_count = current_count;
                    viewer->update_event_count(event_delta);
                }
            }

            // Render UI
            render_status_panel();
            render_camera_views();
            render_help_window();

            // Handle keyboard shortcuts
            if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
                show_help_window = !show_help_window;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                glfwSetWindowShouldClose(window, true);
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
        std::cerr << "Application will now shutdown." << std::endl;
    }

    // Cleanup
    std::cout << "\nShutting down..." << std::endl;

    CameraManager::instance().shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Shutdown complete" << std::endl;
    return 0;
}
