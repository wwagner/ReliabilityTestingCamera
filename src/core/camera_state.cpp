#include "core/camera_state.h"

namespace core {

void CameraState::set_connected(bool connected) {
    camera_connected_.store(connected);
}

bool CameraState::is_connected() const {
    return camera_connected_.load();
}

} // namespace core
