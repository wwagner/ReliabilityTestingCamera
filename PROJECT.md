# Project Name
Reliability Testing Camera Application

## Overview
This application will enable us to view a single event camera, process it for noise evaluation, and store images with processing data for previous images.
The stored image/processing data will be time-stamped and enable the user to include comments.
This application will not be used by an event camera expert- just a regular user.

## Goals and Objectives


## Requirements
-Runs the event camera and shows a view of the event camera image.
-Lets the user load a previous image for comparison.
-Lets the user load a difference image for comparison.
-Encodes scattering data in the image.  These are pixels that are not seen in the original data.
-Pulls event camera settings from an ini file, but does not let the user change these settings.


## Architecture and Design
-Base on EventCamera code in C:\Users\wolfw\source\repos\EventCamera
-Images should be binary only (two bits of 8-bit image set in ini file)
-UI should be similar with two views on top, but only the left view will be connected to an event camera
-We will only ever connect one event camera to this application
-Buttons for 'Calculate Scattering', 'Save Image', 'Load Image', 'Compare Image'
-'Compare Image' overlays existing and loaded image similarly to the 'Add Images' mode in EventCamera.
-in 'Compare Image', statistics for each individual image and the combined image are visible.
-Two image views on top, statistics and processing buttons at the bottom.

## Implementation Plan

### Phase 1: Foundation & Setup ✅ COMPLETED
- ✅ Copy and adapt EventCamera code as starting point
- ✅ Simplify architecture to support single camera only
- ✅ Remove multi-camera connection logic
- ✅ Set up project structure and build system
- ✅ Create INI file parser for camera settings (read-only mode)
- ✅ Configure binary image mode (two-bit threshold settings)

### Phase 2: Core Camera & Display ✅ COMPLETED
- ✅ Implement single event camera connection and initialization
- ✅ Display live camera feed in left view
- ✅ Implement binary image processing pipeline
- ✅ Adapt existing UI to two-view layout (left: live camera, right: comparison)
- ✅ Basic window layout with top views and bottom control panel

### Phase 3: Image Persistence ✅ COMPLETED
- ✅ Implement "Save Image" functionality with:
  - ✅ Timestamp generation
  - ✅ Comment/annotation capability
  - ✅ PNG format with JSON metadata sidecar
- ✅ Implement "Load Image" functionality
- ✅ Design file storage structure for images + metadata
- ✅ Create image file browser/selector with Windows file dialog
- ✅ Independent viewer controls - each viewer can load/save/display independently

### Phase 4: Image Comparison & Statistics ✅ COMPLETED
- ✅ Implement "Compare Image" overlay mode (similar to EventCamera's 'Add Images')
- ✅ Display loaded image in right view
- ✅ Calculate and display statistics for:
  - ✅ Left image (live or frozen)
  - ✅ Right image (loaded)
  - ✅ Combined/overlay image
- ✅ Implement difference image calculation and display

### Phase 5: Scattering Analysis ✅ COMPLETED
- ✅ Implement "Calculate Scattering" algorithm
- ✅ Identify pixels not in original data
- ✅ Encode scattering data into image representation
- ✅ Add scattering metrics to statistics display
- ✅ Visual highlighting of scattered pixels
- ✅ Temporal tracking of scattering over time
- ✅ Export scattering statistics to CSV
- ✅ Export scattering heatmap to PNG

### Phase 6: UI Polish & User Testing 🔄 IN PROGRESS
- ✅ Simplified controls for non-expert users
- ✅ Tooltips and help text
- ✅ Error handling and user feedback
- ✅ Help window with keyboard shortcuts
- ✅ Performance optimization
- ✅ Windows file browser integration
- ⏳ Reorganize viewer-specific settings (PENDING)
- ⏳ Testing with non-expert users
- ⏳ Documentation and user guide

**Key Dependencies:**
- Phase 2 depends on Phase 1 completion ✅
- Phase 4 depends on Phases 2 & 3 ✅
- Phase 5 can be developed in parallel with Phase 4 ✅
- Phase 6 is final integration 🔄

## Testing Strategy

## Timeline and Milestones

## Resources and Dependencies

## Risks and Challenges

## Current Features (as of 2025-01-11)

### Dual Independent Viewers
- **Left Viewer**: Can display live camera feed or loaded images
- **Right Viewer**: Can display live camera feed or loaded images
- Each viewer has independent controls via dropdown menu:
  - **Active Camera**: Show live camera feed
  - **Load Image...**: Browse and load saved PNG images
  - **Save Image...**: Save current view with metadata and comments

### Image Management
- **File Browser Integration**: Native Windows file dialog for easy image selection
- **Automatic Format Conversion**: Handles grayscale images and converts for display
- **Metadata Storage**: Images saved with JSON sidecar files containing:
  - Timestamp (ISO 8601 format)
  - Camera settings (biases, accumulation time, binary bits)
  - Image statistics (resolution, active pixels, density)
  - User comments
- **Smart Loading**: Automatically loads metadata if available, gracefully handles images without metadata

### Image Comparison & Analysis
- **Comparison Modes**:
  - Overlay: Color-coded visualization (Yellow=Both, Green=Live only, Red=Loaded only)
  - Difference: Binary difference highlighting
  - Side-by-Side: Original images for direct comparison
- **Statistics Display**: Comprehensive metrics for both images and differences
- **Scattering Analysis**:
  - Real-time detection of noise pixels not in reference image
  - Temporal tracking of scattering frequency
  - Hot spot detection
  - Export to CSV and PNG heatmap

### Configuration
- **INI-based Settings**: All camera parameters loaded from `event_config.ini`
- **Read-only Mode**: Settings cannot be changed at runtime (prevents accidental misconfiguration)
- **Binary Bit Processing**: Extracts and combines two configurable bits from 8-bit image

### Launch Options
- **start.bat**: Convenient launcher that runs from root directory
- Uses root folder's `event_config.ini`
- Automatic DLL path configuration

## Recent Updates (2025-01-11)

### Image Loading Enhancement
- ✅ Added Windows file browser dialog for easy image selection
- ✅ Fixed image format conversion (grayscale to BGR) preventing crashes
- ✅ Enhanced error handling and debug output for troubleshooting
- ✅ Each viewer now maintains independent state for loaded images

### Code Organization
- ✅ Resolved Windows API macro conflicts (SIDE_BY_SIDE)
- ✅ Proper header ordering to prevent compilation issues
- ✅ Separated file dialog implementation to avoid macro pollution

### Launcher Improvements
- ✅ Replaced old batch files with single `start.bat`
- ✅ Simplified launch process - runs from root directory
- ✅ No duplicate config files needed

## Notes
