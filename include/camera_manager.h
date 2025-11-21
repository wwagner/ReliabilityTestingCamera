#pragma once

#include <metavision/sdk/driver/camera.h>
#include <metavision/sdk/core/algorithms/periodic_frame_generation_algorithm.h>
#include <opencv2/core.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

/**
 * CameraManager handles enumeration, selection, and initialization of SilkyEvCam event cameras.
 * Simplified for single camera operation (Reliability Testing Application).
 *
 * Hardware: CenturyArks SilkyEvCam HD
 * SDK: Metavision (via CenturyArks silky_common_plugin)
 */
class CameraManager {
public:
    // Singleton instance
    static CameraManager& instance() {
        static CameraManager instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // Frame callback type: receives generated frame and camera index
    using FrameCallback = std::function<void(const cv::Mat&, int)>;

    struct CameraInfo {
        std::string serial;
        uint16_t width;
        uint16_t height;
        std::unique_ptr<Metavision::Camera> camera;

        CameraInfo(const std::string& s, uint16_t w, uint16_t h, std::unique_ptr<Metavision::Camera> cam)
            : serial(s), width(w), height(h), camera(std::move(cam)) {}
    };

    /**
     * Initialize cameras based on serial numbers.
     * @param serial1 Serial number for first camera (empty = auto-detect)
     * @param serial2 Serial number for second camera (empty = no second camera)
     * @return Number of cameras successfully initialized
     */
    int initialize(const std::string& serial1 = "", const std::string& serial2 = "");

    /**
     * Get number of initialized cameras
     */
    int num_cameras() const { return static_cast<int>(cameras_.size()); }

    /**
     * Get camera info by index
     */
    CameraInfo& get_camera(int index) { return cameras_[index]; }
    const CameraInfo& get_camera(int index) const { return cameras_[index]; }

    /**
     * Get all cameras
     */
    std::vector<CameraInfo>& get_cameras() { return cameras_; }
    const std::vector<CameraInfo>& get_cameras() const { return cameras_; }

    /**
     * Initialize single camera (simplified for reliability testing)
     * @param accumulation_time_us Frame accumulation period in microseconds
     * @return true if successful
     */
    bool initialize_single_camera(int accumulation_time_us);

    /**
     * Start the single camera with frame generation
     * @param callback Frame callback function
     * @return true if successful
     */
    bool start_single_camera(FrameCallback callback);

    /**
     * Check if camera is connected
     * @param index Camera index (0 for single camera)
     * @return true if connected
     */
    bool is_camera_connected(int index = 0) const;

    /**
     * Shutdown cameras and clean up
     */
    void shutdown();

    /**
     * List all available camera serial numbers
     */
    static std::vector<std::string> list_available_cameras();

    /**
     * Get current event count (for focus adjust monitoring)
     */
    uint64_t get_event_count() const { return event_count_; }

private:
    CameraManager() = default;

public:
    ~CameraManager() = default;

private:
    std::vector<CameraInfo> cameras_;

    // Frame generation for single camera
    std::unique_ptr<Metavision::PeriodicFrameGenerationAlgorithm> frame_generator_;
    FrameCallback frame_callback_;
    bool camera_started_ = false;

    // Event counting for focus adjust
    std::atomic<uint64_t> event_count_{0};

    /**
     * Open camera by serial number or index
     */
    std::unique_ptr<Metavision::Camera> open_camera(const std::string& serial);

    /**
     * Open first available camera
     */
    std::unique_ptr<Metavision::Camera> open_first_available();
};
