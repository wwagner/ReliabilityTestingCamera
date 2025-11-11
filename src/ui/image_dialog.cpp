/**
 * @file image_dialog.cpp
 * @brief Implementation of unified image dialog system
 */

#include "ui/image_dialog.h"
#include "app_config.h"
#include "imgui.h"
#include <iostream>
#include <opencv2/imgproc.hpp>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace ui {

// ============================================================================
// Private Helper: Native File Browser
// ============================================================================

bool ImageDialog::open_file_browser(std::string& filepath) {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[512] = "";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        filepath = ofn.lpstrFile;
        return true;
    }
    return false;
#else
    // Non-Windows platforms: user must type path manually
    return false;
#endif
}

// ============================================================================
// Load Dialog
// ============================================================================

bool ImageDialog::show_load_dialog(
    const std::string& dialog_id,
    LoadDialogState& state,
    LoadResult& result
) {
    // Open popup if requested
    if (state.show_dialog) {
        ImGui::OpenPopup(dialog_id.c_str());
        state.show_dialog = false;
    }

    bool loaded_this_frame = false;

    // Render dialog
    if (ImGui::BeginPopupModal(dialog_id.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Load Image");
        ImGui::Separator();

        // File path input
        ImGui::Text("Image file path:");
        ImGui::PushItemWidth(400);

        // Convert std::string to char buffer for ImGui
        static char filepath_buffer[512] = "";
        if (state.filepath.empty()) {
            filepath_buffer[0] = '\0';
        } else {
            strncpy_s(filepath_buffer, sizeof(filepath_buffer), state.filepath.c_str(), sizeof(filepath_buffer) - 1);
        }

        std::string input_id = "##FilePath_" + dialog_id;
        if (ImGui::InputText(input_id.c_str(), filepath_buffer, sizeof(filepath_buffer))) {
            state.filepath = filepath_buffer;
        }
        ImGui::PopItemWidth();

        // Browse button
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            if (open_file_browser(state.filepath)) {
                std::cout << "Selected file: " << state.filepath << std::endl;
                strncpy_s(filepath_buffer, sizeof(filepath_buffer), state.filepath.c_str(), sizeof(filepath_buffer) - 1);
            }
        }

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Click Browse to select a file, or enter the full path manually");

        ImGui::Separator();

        // Load button
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (!state.filepath.empty()) {
                std::cout << "Attempting to load: " << state.filepath << std::endl;

                if (ImageManager::load_image(state.filepath, result.metadata, result.image)) {
                    std::cout << "Image loaded successfully" << std::endl;
                    std::cout << "  Size: " << result.image.cols << "x" << result.image.rows << std::endl;
                    std::cout << "  Channels: " << result.image.channels() << std::endl;

                    result.filepath = state.filepath;
                    result.success = true;
                    loaded_this_frame = true;

                    state.reset();  // Clear for next use
                } else {
                    std::cerr << "Failed to load image: " << state.filepath << std::endl;
                    result.success = false;
                }
            }

            ImGui::CloseCurrentPopup();
        }

        // Cancel button
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            state.reset();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return loaded_this_frame;
}

// ============================================================================
// Save Dialog
// ============================================================================

bool ImageDialog::show_save_dialog(
    const std::string& dialog_id,
    SaveDialogState& state,
    const cv::Mat& image_to_save,
    std::string& saved_path
) {
    // Open popup if requested
    if (state.show_dialog) {
        ImGui::OpenPopup(dialog_id.c_str());
        state.show_dialog = false;
    }

    bool saved_this_frame = false;

    // Render dialog
    if (ImGui::BeginPopupModal(dialog_id.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save Image");
        ImGui::Separator();

        // Check if there's an image to save
        if (image_to_save.empty()) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No image to save!");
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
        } else {
            // Comment input
            ImGui::Text("Comment:");

            // Convert std::string to char buffer for ImGui
            static char comment_buffer[512] = "";
            if (state.comment.empty()) {
                comment_buffer[0] = '\0';
            } else {
                strncpy_s(comment_buffer, sizeof(comment_buffer), state.comment.c_str(), sizeof(comment_buffer) - 1);
            }

            std::string comment_id = "##Comment_" + dialog_id;
            if (ImGui::InputTextMultiline(comment_id.c_str(), comment_buffer, sizeof(comment_buffer), ImVec2(400, 100))) {
                state.comment = comment_buffer;
            }

            ImGui::Separator();

            // Save button
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                auto& config = AppConfig::instance();

                // Create metadata with user comment
                auto metadata = ImageManager::create_metadata(image_to_save, state.comment);

                // Save image and metadata
                saved_path = ImageManager::save_image(
                    image_to_save,
                    metadata,
                    config.camera_settings().capture_directory,
                    "capture"
                );

                if (!saved_path.empty()) {
                    std::cout << "Image saved: " << saved_path << std::endl;
                    saved_this_frame = true;
                    state.reset();  // Clear for next use
                } else {
                    std::cerr << "Failed to save image" << std::endl;
                }

                ImGui::CloseCurrentPopup();
            }

            // Cancel button
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                state.reset();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    return saved_this_frame;
}

} // namespace ui
