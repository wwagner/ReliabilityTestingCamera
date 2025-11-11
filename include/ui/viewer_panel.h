/**
 * @file viewer_panel.h
 * @brief Self-contained viewer panel for displaying camera feeds or loaded images
 *
 * Encapsulates all viewer state and rendering logic that was previously in main.cpp.
 * Part of Phase 1.2 refactoring to reduce main.cpp size.
 */

#pragma once

#include <string>
#include <memory>
#include <opencv2/core.hpp>
#include "image_manager.h"
#include "video/texture_manager.h"
#include "noise_analyzer.h"
#include "ui/image_dialog.h"

namespace ui {

/**
 * @brief Viewer display mode
 */
enum class ViewerMode {
    ACTIVE_CAMERA,   // Show live camera feed
    LOADED_IMAGE     // Show loaded static image
};

/**
 * @brief Self-contained viewer panel
 *
 * Manages a single viewer that can display either:
 * - Live camera feed from shared camera buffer
 * - Loaded static image from disk
 *
 * Includes integrated noise analysis, image load/save, and visualization.
 */
class ViewerPanel {
public:
    /**
     * @brief Construct a new Viewer Panel
     *
     * @param name Display name (e.g., "Left Viewer", "Right Viewer")
     */
    explicit ViewerPanel(const std::string& name);

    /**
     * @brief Destroy the Viewer Panel
     */
    ~ViewerPanel();

    /**
     * @brief Render the viewer panel
     *
     * @param camera_frame Shared camera frame buffer (for ACTIVE_CAMERA mode)
     * @param camera_texture_id OpenGL texture ID for camera feed (0 if none)
     * @param camera_width Camera texture width
     * @param camera_height Camera texture height
     */
    void render(const cv::Mat& camera_frame, GLuint camera_texture_id = 0,
                int camera_width = 0, int camera_height = 0);

    /**
     * @brief Get viewer name
     */
    const std::string& get_name() const { return name_; }

    /**
     * @brief Get current viewer mode
     */
    ViewerMode get_mode() const { return mode_; }

    /**
     * @brief Get loaded image (empty if none loaded)
     */
    const cv::Mat& get_loaded_image() const { return loaded_image_; }

private:
    // Identity
    std::string name_;

    // Display mode
    ViewerMode mode_ = ViewerMode::ACTIVE_CAMERA;
    int selected_mode_index_ = 0;  // For dropdown

    // Loaded image state
    cv::Mat loaded_image_;
    ImageManager::ImageMetadata loaded_metadata_;
    std::unique_ptr<video::TextureManager> texture_;
    std::string last_loaded_path_;

    // Image dialogs
    LoadDialogState load_dialog_;
    SaveDialogState save_dialog_;

    // Noise analysis state
    std::unique_ptr<NoiseAnalyzer> noise_analyzer_;
    NoiseAnalysisResults noise_results_;
    bool noise_analysis_complete_ = false;
    DotDetectionParams noise_params_;
    std::unique_ptr<video::TextureManager> noise_viz_texture_;

    /**
     * @brief Render mode dropdown and controls
     */
    void render_mode_controls();

    /**
     * @brief Render the current image (camera or loaded)
     *
     * @param camera_frame Shared camera frame buffer
     * @param camera_tex_id OpenGL texture ID for camera feed
     * @param cam_width Camera texture width
     * @param cam_height Camera texture height
     */
    void render_image(const cv::Mat& camera_frame, GLuint camera_tex_id,
                     int cam_width, int cam_height);

    /**
     * @brief Render noise analysis section
     *
     * @param camera_frame Shared camera frame buffer (for ACTIVE_CAMERA mode)
     */
    void render_noise_analysis(const cv::Mat& camera_frame);

    /**
     * @brief Handle load image dialog
     */
    void handle_load_dialog();

    /**
     * @brief Handle save image dialog
     *
     * @param camera_frame Shared camera frame buffer (for ACTIVE_CAMERA mode)
     */
    void handle_save_dialog(const cv::Mat& camera_frame);

    /**
     * @brief Get dialog ID for this viewer
     */
    std::string get_dialog_id(const std::string& dialog_type) const {
        return dialog_type + "##" + name_;
    }
};

} // namespace ui
