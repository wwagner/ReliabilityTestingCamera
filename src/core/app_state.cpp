#include "core/app_state.h"

namespace core {

AppState::AppState() {
    // Initialize video subsystems for single camera
    for (int i = 0; i < MAX_CAMERAS; ++i) {
        frame_buffers_[i] = std::make_unique<video::FrameBuffer>();
        texture_managers_[i] = std::make_unique<video::TextureManager>();
    }

    // Initialize core subsystems
    display_settings_ = std::make_unique<DisplaySettings>();
    camera_state_ = std::make_unique<CameraState>();
}

AppState::~AppState() = default;

video::FrameBuffer& AppState::frame_buffer(int camera_index) {
    return *frame_buffers_[camera_index];
}

video::TextureManager& AppState::texture_manager(int camera_index) {
    return *texture_managers_[camera_index];
}

DisplaySettings& AppState::display_settings() {
    return *display_settings_;
}

CameraState& AppState::camera_state() {
    return *camera_state_;
}

bool AppState::is_running() const {
    return running_.load();
}

void AppState::request_shutdown() {
    running_.store(false);
}

void AppState::reset_running_flag() {
    running_.store(true);
}

} // namespace core
