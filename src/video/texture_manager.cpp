#include "video/texture_manager.h"

namespace video {

TextureManager::TextureManager() {
    // Texture will be created on first upload
}

TextureManager::~TextureManager() {
    if (texture_id_ != 0) {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
}

void TextureManager::ensure_texture_created() {
    if (texture_id_ == 0) {
        glGenTextures(1, &texture_id_);
        glBindTexture(GL_TEXTURE_2D, texture_id_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, texture_id_);
    }
}

void TextureManager::upload_frame(const cv::Mat& frame) {
    if (frame.empty()) {
        return;
    }

    ensure_texture_created();

    // Convert to RGB format for OpenGL
    cv::Mat rgb_frame;
    if (frame.channels() == 1) {
        // Convert grayscale to RGB
        cv::cvtColor(frame, rgb_frame, cv::COLOR_GRAY2RGB);
    } else if (frame.channels() == 3) {
        // If the frame is BGR, convert to RGB
        if (frame.type() == CV_8UC3) {
            cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);
        } else {
            rgb_frame = frame;
        }
    } else {
        rgb_frame = frame;
    }

    // Upload to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb_frame.cols, rgb_frame.rows,
                 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_frame.data);

    width_ = rgb_frame.cols;
    height_ = rgb_frame.rows;

    // ZERO-COPY: Store FrameRef (shares data, no clone!)
    last_frame_ = FrameRef(frame);
}

void TextureManager::upload_frame(const FrameRef& frame_ref) {
    if (frame_ref.empty()) {
        return;
    }

    // ZERO-COPY: Get read access to frame data
    ReadGuard guard(frame_ref);
    const cv::Mat& frame = guard.get();

    if (frame.empty()) {
        return;
    }

    // Validate frame data
    if (frame.data == nullptr || frame.cols <= 0 || frame.rows <= 0) {
        std::cerr << "Invalid frame data in upload_frame" << std::endl;
        return;
    }

    ensure_texture_created();

    // Convert to RGB format for OpenGL
    // For single-channel grayscale, convert to RGB
    cv::Mat rgb_frame;
    if (frame.channels() == 1) {
        // Convert grayscale to RGB
        cv::cvtColor(frame, rgb_frame, cv::COLOR_GRAY2RGB);
    } else if (frame.channels() == 3) {
        if (frame.type() == CV_8UC3) {
            cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);
        } else {
            rgb_frame = frame;
        }
    } else {
        rgb_frame = frame;
    }

    // Final validation before GPU upload
    if (rgb_frame.empty() || rgb_frame.data == nullptr) {
        std::cerr << "RGB frame is empty after conversion" << std::endl;
        return;
    }

    // Upload to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb_frame.cols, rgb_frame.rows,
                 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_frame.data);

    width_ = rgb_frame.cols;
    height_ = rgb_frame.rows;

    // ZERO-COPY: Store FrameRef (shares data, no clone!)
    last_frame_ = frame_ref;
}

void TextureManager::reset() {
    if (texture_id_ != 0) {
        glDeleteTextures(1, &texture_id_);
        texture_id_ = 0;
    }
    width_ = 0;
    height_ = 0;
    last_frame_.reset();  // Clear stored frame reference
}

} // namespace video
