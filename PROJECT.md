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

### Phase 1: Foundation & Setup (Highest Priority)
- Copy and adapt EventCamera code as starting point
- Simplify architecture to support single camera only
- Remove multi-camera connection logic
- Set up project structure and build system
- Create INI file parser for camera settings (read-only mode)
- Configure binary image mode (two-bit threshold settings)

### Phase 2: Core Camera & Display
- Implement single event camera connection and initialization
- Display live camera feed in left view
- Implement binary image processing pipeline
- Adapt existing UI to two-view layout (left: live camera, right: comparison)
- Basic window layout with top views and bottom control panel

### Phase 3: Image Persistence
- Implement "Save Image" functionality with:
  - Timestamp generation
  - Comment/annotation capability
  - File format selection (with metadata)
- Implement "Load Image" functionality
- Design file storage structure for images + metadata
- Create image file browser/selector

### Phase 4: Image Comparison & Statistics
- Implement "Compare Image" overlay mode (similar to EventCamera's 'Add Images')
- Display loaded image in right view
- Calculate and display statistics for:
  - Left image (live or frozen)
  - Right image (loaded)
  - Combined/overlay image
- Implement difference image calculation and display

### Phase 5: Scattering Analysis
- Implement "Calculate Scattering" algorithm
- Identify pixels not in original data
- Encode scattering data into image representation
- Add scattering metrics to statistics display
- Visual highlighting of scattered pixels

### Phase 6: UI Polish & User Testing
- Simplify controls for non-expert users
- Add tooltips and help text
- Error handling and user feedback
- Testing with non-expert users
- Performance optimization
- Documentation and user guide

**Key Dependencies:**
- Phase 2 depends on Phase 1 completion
- Phase 4 depends on Phases 2 & 3
- Phase 5 can be developed in parallel with Phase 4
- Phase 6 is final integration

## Testing Strategy

## Timeline and Milestones

## Resources and Dependencies

## Risks and Challenges

## Notes
