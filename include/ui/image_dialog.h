/**
 * @file image_dialog.h
 * @brief Unified dialog system for loading and saving images
 *
 * Replaces duplicated dialog handler functions with a clean, reusable class.
 */

#pragma once

#include <string>
#include <functional>
#include <opencv2/core.hpp>
#include "image_manager.h"
#include "video/texture_manager.h"

namespace ui {

/**
 * @brief State for a load image dialog
 */
struct LoadDialogState {
    bool show_dialog = false;
    std::string filepath;

    void open() { show_dialog = true; }
    void reset() { filepath.clear(); }
};

/**
 * @brief State for a save image dialog
 */
struct SaveDialogState {
    bool show_dialog = false;
    std::string comment;

    void open() { show_dialog = true; }
    void reset() { comment.clear(); }
};

/**
 * @brief Result of a load operation
 */
struct LoadResult {
    bool success = false;
    cv::Mat image;
    ImageManager::ImageMetadata metadata;
    std::string filepath;
};

/**
 * @brief Unified image dialog system
 *
 * Handles both load and save dialogs with proper state management.
 * Eliminates 300+ lines of duplicated code from main.cpp.
 */
class ImageDialog {
public:
    /**
     * @brief Show load image dialog
     *
     * @param dialog_id Unique ID for this dialog (e.g., "LoadDialog_LeftViewer")
     * @param state Dialog state (persistent across frames)
     * @param result Output - populated on successful load
     * @return true if user loaded an image this frame
     */
    static bool show_load_dialog(
        const std::string& dialog_id,
        LoadDialogState& state,
        LoadResult& result
    );

    /**
     * @brief Show save image dialog
     *
     * @param dialog_id Unique ID for this dialog (e.g., "SaveDialog_LeftViewer")
     * @param state Dialog state (persistent across frames)
     * @param image_to_save Image to save
     * @param saved_path Output - path where image was saved
     * @return true if user saved the image this frame
     */
    static bool show_save_dialog(
        const std::string& dialog_id,
        SaveDialogState& state,
        const cv::Mat& image_to_save,
        std::string& saved_path
    );

private:
    /**
     * @brief Open native Windows file browser
     *
     * @param filepath Output - selected file path
     * @return true if user selected a file
     */
    static bool open_file_browser(std::string& filepath);
};

} // namespace ui
