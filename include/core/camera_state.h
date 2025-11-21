#pragma once

#include <atomic>

namespace core {

/**
 * Camera connection state (simplified for single camera)
 */
class CameraState {
public:
    CameraState() = default;
    ~CameraState() = default;

    // Non-copyable
    CameraState(const CameraState&) = delete;
    CameraState& operator=(const CameraState&) = delete;

    /**
     * Set connection state
     * @param connected true if camera is connected
     */
    void set_connected(bool connected);

    /**
     * Check if camera is connected
     * @return true if connected
     */
    bool is_connected() const;

private:
    std::atomic<bool> camera_connected_{false};
};

} // namespace core
