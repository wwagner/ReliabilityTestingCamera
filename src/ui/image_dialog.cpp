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

bool ImageDialog::open_save_file_browser(std::string& filepath, const std::string& initial_dir) {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[512] = "";

    // If there's a suggested filename, use it
    if (!filepath.empty()) {
        strncpy_s(szFile, sizeof(szFile), filepath.c_str(), sizeof(szFile) - 1);
    }

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = initial_dir.c_str();
    ofn.lpstrDefExt = "png";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn) == TRUE) {
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
            // File path input
            ImGui::Text("Save location:");
            ImGui::PushItemWidth(400);

            // Convert std::string to char buffer for ImGui
            static char filepath_buffer[512] = "";
            if (state.filepath.empty()) {
                // Default to C:\ with a suggested filename
                state.filepath = "C:\\image.png";
            }
            strncpy_s(filepath_buffer, sizeof(filepath_buffer), state.filepath.c_str(), sizeof(filepath_buffer) - 1);

            std::string filepath_id = "##FilePath_" + dialog_id;
            if (ImGui::InputText(filepath_id.c_str(), filepath_buffer, sizeof(filepath_buffer))) {
                state.filepath = filepath_buffer;
            }
            ImGui::PopItemWidth();

            // Browse button
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                if (open_save_file_browser(state.filepath, "C:\\")) {
                    std::cout << "Selected save location: " << state.filepath << std::endl;
                    strncpy_s(filepath_buffer, sizeof(filepath_buffer), state.filepath.c_str(), sizeof(filepath_buffer) - 1);
                }
            }

            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "Click Browse to select location, or enter the full path manually");

            ImGui::Separator();

            // Comment input
            ImGui::Text("Comment (optional):");

            // Convert std::string to char buffer for ImGui
            static char comment_buffer[512] = "";
            if (state.comment.empty()) {
                comment_buffer[0] = '\0';
            } else {
                strncpy_s(comment_buffer, sizeof(comment_buffer), state.comment.c_str(), sizeof(comment_buffer) - 1);
            }

            std::string comment_id = "##Comment_" + dialog_id;
            if (ImGui::InputTextMultiline(comment_id.c_str(), comment_buffer, sizeof(comment_buffer), ImVec2(400, 80))) {
                state.comment = comment_buffer;
            }

            ImGui::Separator();

            // Save button
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                if (!state.filepath.empty()) {
                    // Create metadata with user comment
                    auto metadata = ImageManager::create_metadata(image_to_save, state.comment);

                    // Try to write the image directly to the specified path
                    try {
                        if (cv::imwrite(state.filepath, image_to_save)) {
                            saved_path = state.filepath;
                            std::cout << "Image saved: " << saved_path << std::endl;

                            // Save metadata JSON alongside the image
                            std::string json_path = state.filepath.substr(0, state.filepath.find_last_of('.')) + ".json";
                            if (ImageManager::save_metadata_json(json_path, metadata)) {
                                std::cout << "Metadata saved: " << json_path << std::endl;
                            } else {
                                std::cerr << "Warning: Failed to save metadata JSON" << std::endl;
                            }

                            saved_this_frame = true;
                            state.reset();  // Clear for next use
                        } else {
                            std::cerr << "Failed to save image to: " << state.filepath << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error saving image: " << e.what() << std::endl;
                    }
                } else {
                    std::cerr << "No filepath specified" << std::endl;
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
