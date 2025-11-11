#include "camera_manager.h"
#include <metavision/hal/device/device_discovery.h>
#include <iostream>
#include <stdexcept>

int CameraManager::initialize(const std::string& serial1, const std::string& serial2) {
    cameras_.clear();

    try {
        // Case 1: Both serials specified
        if (!serial1.empty() && !serial2.empty()) {
            std::cout << "Opening cameras with serial numbers: " << serial1 << ", " << serial2 << std::endl;

            auto cam1 = open_camera(serial1);
            if (cam1) {
                const auto& geom1 = cam1->geometry();
                cameras_.emplace_back(serial1, geom1.width(), geom1.height(), std::move(cam1));
                std::cout << "Camera 1 opened: " << serial1 << " (" << geom1.width() << "x" << geom1.height() << ")" << std::endl;
            }

            auto cam2 = open_camera(serial2);
            if (cam2) {
                const auto& geom2 = cam2->geometry();
                cameras_.emplace_back(serial2, geom2.width(), geom2.height(), std::move(cam2));
                std::cout << "Camera 2 opened: " << serial2 << " (" << geom2.width() << "x" << geom2.height() << ")" << std::endl;
            }
        }
        // Case 2: Only first serial specified
        else if (!serial1.empty() && serial2.empty()) {
            std::cout << "Opening camera with serial number: " << serial1 << std::endl;

            auto cam1 = open_camera(serial1);
            if (cam1) {
                const auto& geom1 = cam1->geometry();
                cameras_.emplace_back(serial1, geom1.width(), geom1.height(), std::move(cam1));
                std::cout << "Camera opened: " << serial1 << " (" << geom1.width() << "x" << geom1.height() << ")" << std::endl;
            }
        }
        // Case 3: Auto-detect (no serials specified)
        else {
            std::cout << "Auto-detecting cameras..." << std::endl;

            auto available = list_available_cameras();
            std::cout << "Found " << available.size() << " camera(s)" << std::endl;

            if (available.empty()) {
                std::cerr << "No cameras detected!" << std::endl;
                return 0;
            }

            // Open first camera
            auto cam1 = open_camera(available[0]);
            if (cam1) {
                const auto& geom1 = cam1->geometry();
                cameras_.emplace_back(available[0], geom1.width(), geom1.height(), std::move(cam1));
                std::cout << "Camera 1 opened: " << available[0] << " (" << geom1.width() << "x" << geom1.height() << ")" << std::endl;
            }

            // Open second camera if available
            if (available.size() >= 2) {
                auto cam2 = open_camera(available[1]);
                if (cam2) {
                    const auto& geom2 = cam2->geometry();
                    cameras_.emplace_back(available[1], geom2.width(), geom2.height(), std::move(cam2));
                    std::cout << "Camera 2 opened: " << available[1] << " (" << geom2.width() << "x" << geom2.height() << ")" << std::endl;
                }
            }
        }

    } catch (const Metavision::CameraException& e) {
        std::cerr << "Camera initialization error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return num_cameras();
}

std::vector<std::string> CameraManager::list_available_cameras() {
    std::vector<std::string> serials;

    try {
        // Use list() to get camera serials
        auto serial_list = Metavision::DeviceDiscovery::list();

        for (const auto& serial : serial_list) {
            serials.push_back(serial);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error listing cameras: " << e.what() << std::endl;
    }

    return serials;
}

std::unique_ptr<Metavision::Camera> CameraManager::open_camera(const std::string& serial) {
    try {
        auto camera = std::make_unique<Metavision::Camera>(
            Metavision::Camera::from_serial(serial)
        );
        return camera;
    } catch (const Metavision::CameraException& e) {
        std::cerr << "Failed to open camera " << serial << ": " << e.what() << std::endl;
        return nullptr;
    }
}

std::unique_ptr<Metavision::Camera> CameraManager::open_first_available() {
    try {
        auto camera = std::make_unique<Metavision::Camera>(
            Metavision::Camera::from_first_available()
        );
        return camera;
    } catch (const Metavision::CameraException& e) {
        std::cerr << "Failed to open first available camera: " << e.what() << std::endl;
        return nullptr;
    }
}

bool CameraManager::initialize_single_camera(int accumulation_time_us) {
    std::cout << "Initializing single camera..." << std::endl;

    try {
        // List available cameras
        auto available = list_available_cameras();

        if (available.empty()) {
            std::cerr << "No cameras detected!" << std::endl;
            return false;
        }

        std::cout << "Found " << available.size() << " camera(s)" << std::endl;
        std::cout << "Using first camera: " << available[0] << std::endl;

        // Open first camera
        auto camera = open_camera(available[0]);
        if (!camera) {
            std::cerr << "Failed to open camera" << std::endl;
            return false;
        }

        // Get camera geometry
        const auto& geom = camera->geometry();
        std::cout << "Camera resolution: " << geom.width() << "x" << geom.height() << std::endl;

        // Create frame generator
        frame_generator_ = std::make_unique<Metavision::PeriodicFrameGenerationAlgorithm>(
            geom.width(), geom.height(), accumulation_time_us);

        std::cout << "Frame generator created (accumulation: " << accumulation_time_us << " μs)" << std::endl;

        // Store camera info
        cameras_.clear();
        cameras_.emplace_back(available[0], geom.width(), geom.height(), std::move(camera));

        std::cout << "Camera initialized successfully (not started yet)" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Camera initialization error: " << e.what() << std::endl;
        return false;
    }
}

bool CameraManager::start_single_camera(FrameCallback callback) {
    if (cameras_.empty() || !frame_generator_) {
        std::cerr << "Camera not initialized. Call initialize_single_camera() first." << std::endl;
        return false;
    }

    try {
        std::cout << "Setting up camera callbacks..." << std::endl;

        // Store callback
        frame_callback_ = callback;

        // Set up frame generator output callback
        frame_generator_->set_output_callback(
            [this](const Metavision::timestamp ts, cv::Mat& frame) {
                if (frame.empty()) return;
                if (frame_callback_) {
                    frame_callback_(frame, 0);  // Camera index 0
                }
            });

        // Set up event callback to feed events to frame generator
        auto& camera = cameras_[0].camera;
        camera->cd().add_callback(
            [this](const Metavision::EventCD* begin, const Metavision::EventCD* end) {
                if (begin == end) return;
                if (frame_generator_) {
                    frame_generator_->process_events(begin, end);
                }
            });

        std::cout << "Camera callbacks configured (camera not started yet)" << std::endl;

        // Now start the camera
        std::cout << "Starting camera..." << std::endl;
        camera->start();
        camera_started_ = true;
        std::cout << "Camera started successfully" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error starting camera: " << e.what() << std::endl;
        return false;
    }
}

bool CameraManager::is_camera_connected(int index) const {
    return index == 0 && !cameras_.empty() && camera_started_;
}

void CameraManager::shutdown() {
    std::cout << "Shutting down camera manager..." << std::endl;

    // Stop cameras
    for (auto& cam_info : cameras_) {
        if (cam_info.camera) {
            try {
                cam_info.camera->stop();
                std::cout << "Camera stopped: " << cam_info.serial << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error stopping camera: " << e.what() << std::endl;
            }
        }
    }

    // Clear resources
    frame_generator_.reset();
    cameras_.clear();
    camera_started_ = false;

    std::cout << "Camera manager shutdown complete" << std::endl;
}
