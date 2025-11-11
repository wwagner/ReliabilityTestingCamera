# Reliability Testing Camera

A specialized event camera viewer for reliability testing and noise evaluation of CenturyArks SilkyEvCam HD event cameras.

## Quick Start

1. **Start the Application**:
   - Double-click `start_reliability_camera.bat`
   - Or run: `build\bin\Release\reliability_testing_camera.exe`

2. **First Use**:
   - Press **F1** for help
   - Camera status shown in control panel (green = connected)
   - Three windows: Camera Views (center), Image Statistics (left), Controls (right)
   - Default configuration: bits 0 and 7 displayed
   - Configure settings in `event_config.ini` if needed

3. **Basic Workflow**:
   - Save baseline image → Load as reference → Analyze scattering
   - See Help window (F1) for detailed instructions

4. **If Camera Views window is missing**:
   - Look for "Camera Views" title bar at the top
   - Try clicking and dragging in the black area
   - Delete `imgui.ini` and restart to reset window positions

## Overview

This application enables non-expert users to:
- View live event camera feed with binary image processing
- Save images with timestamps and metadata
- Compare live feed with reference images
- Detect and track noise pixels (scattering analysis)
- Export data for analysis (CSV statistics, PNG heatmaps)

**Key Features**:
- Binary image mode (configurable bit extraction)
- Three comparison modes (Overlay, Difference, Side-by-Side)
- Real-time scattering detection with temporal tracking
- Heatmap visualization of noise patterns
- Comprehensive statistics and data export
- User-friendly interface with tooltips and integrated help

## System Requirements

- **Hardware**: CenturyArks SilkyEvCam HD event camera
- **OS**: Windows 10/11
- **Dependencies**: Metavision SDK, OpenCV 4.8, OpenGL/GLFW

## Configuration

All settings are in `event_config.ini` (restart application after changes):

### Essential Settings

```ini
[Camera]
# Binary image bit positions (key feature)
binary_bit_1 = 0                  # First bit (0-7) - Default: bit 0
binary_bit_2 = 7                  # Second bit (0-7) - Default: bit 7

# Frame rate
accumulation_time_us = 10000      # 10ms = ~100 FPS

# Camera biases
bias_diff_on = 20                 # ON event threshold
bias_diff_off = 20                # OFF event threshold
bias_hpf = 100                    # High-pass filter

# Storage
capture_directory = C:\Users\wolfw\OneDrive\Desktop\ReliabilityTesting
```

See `event_config.ini` for complete settings with detailed explanations.

## Features & Usage

### 1. Image Capture & Management

**Save Image**:
- Captures current camera frame with timestamp
- Stores metadata (camera settings, user comments)
- Format: `YYYY-MM-DDTHH-MM-SS_reliability_test.png` + `.json`

**Load Image**:
- Loads previously saved image for comparison
- Automatically loads metadata if available
- Supports images with or without metadata

### 2. Image Comparison

Compare live camera feed with loaded reference image.

**Overlay Mode** (default):
- 🟡 Yellow = Pixels in both images (agreement)
- 🟢 Green = Pixels only in live (new activity)
- 🔴 Red = Pixels only in loaded (missing activity)

**Difference Mode**:
- White = Different pixels
- Black = Identical pixels

**Side-by-Side Mode**:
- Left: Live camera (updating)
- Right: Loaded reference (static)

**Statistics Provided**:
- Pixels in both images
- Pixels in live only
- Pixels in loaded only
- Difference count and percentage

### 3. Scattering Analysis

Detects **scattering pixels** - noise pixels that appear in live camera but NOT in reference image.

**Starting Analysis**:
1. Load reference/baseline image
2. Click "Start Scattering Analysis"
3. Analysis runs continuously on each frame

**Visualization Modes**:

**Highlight Mode**:
- Scattering pixels shown in magenta
- Real-time noise detection
- Instant visual feedback

**Heatmap Mode**:
- Blue = Low scattering frequency
- Red = High scattering frequency
- Shows cumulative noise patterns

**Statistics Tracked**:
- Current frame: Scattering pixels and percentage
- Temporal: Frames analyzed, total events, average per frame
- Hot spots: Most frequently scattering pixels

**Reset Temporal Data**:
- Clears accumulated counts
- Keeps reference image
- Start fresh tracking session

### 4. Data Export

**Export Statistics (CSV)**:
- Scattering metrics and temporal tracking
- Optional custom filename
- Auto-timestamped: `scattering_stats_YYYY-MM-DDTHH-MM-SS.csv`

**Export Heatmap (PNG)**:
- Frequency visualization image
- Full resolution
- Auto-timestamped: `YYYY-MM-DDTHH-MM-SS_scattering_heatmap.png`

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **F1** | Toggle Help window |
| **ESC** | Close application |

## Common Use Cases

### Sensor Reliability Testing

Monitor camera noise over extended period:
1. Capture clean baseline (no activity/noise)
2. Load baseline as reference
3. Start scattering analysis
4. Run for hours/days
5. Monitor scattering percentage (should stay low)
6. Export statistics for reporting

### Noise Characterization

Quantify sensor noise:
1. Dark frame as reference (lens cap on)
2. Run camera in same conditions
3. Scattering pixels = thermal/dark current noise
4. Heatmap shows noise distribution
5. Hot spots indicate pixel defects

### Environmental Impact Testing

Test temperature/humidity effects:
1. Baseline at room temperature
2. Start scattering analysis
3. Change environmental conditions
4. Watch scattering metrics change
5. Reset temporal data between tests
6. Compare results across conditions

### Long-Term Drift Detection

Monitor sensor aging:
1. Daily baseline comparison
2. Track scattering percentage over weeks/months
3. Increasing trend = degradation
4. Hot spots = failing pixels

## Troubleshooting

### Camera Not Connected

**Symptoms**: Status shows "Disconnected" (red)

**Solutions**:
- Check USB connection
- Verify camera is powered
- Restart application
- Check Metavision SDK installation

### Buttons Grayed Out

**Symptoms**: Cannot click certain buttons

**Solutions**:
- Hover over button for tooltip (explains why disabled)
- Common reasons:
  - "Save Image" requires camera feed
  - "Compare Images" requires loaded image
  - "Start Scattering" requires reference image

### High Scattering Percentage

**Symptoms**: Scattering > 5%

**Possible Causes**:
- Scene changed between baseline and live
- Temperature drift
- Camera noise increased
- Wrong reference image loaded

**Solutions**:
- Verify reference image is appropriate
- Check environmental conditions
- Recapture baseline
- Adjust camera biases in INI file

### Flicker Under LED Lights

**Symptoms**: 50/60 Hz flickering visible

**Solution**: Enable anti-flicker in `event_config.ini`:
```ini
antiflicker_enabled = 1
antiflicker_freq_hz = 60    # 50 for EU, 60 for US
```

## Understanding Statistics

### Comparison Statistics

- **In Both**: Pixels active in both images (reliable/stable)
- **Live Only**: New activity (could be noise or real changes)
- **Loaded Only**: Missing activity (degradation or scene change)
- **Difference %**: Overall change metric (lower = more similar)

### Scattering Statistics

- **Current Scattering Pixels**: Noise in current frame
- **Scattering %**: Noise severity (< 0.5% = good, > 2% = bad)
- **Total Scattering Events**: Cumulative noise across all frames
- **Average Per Frame**: Temporal stability (low = stable)
- **Hot Spot**: Most problematic pixel location and frequency

## File Formats

### Saved Images

- **PNG**: `2025-11-11T14-30-45_reliability_test.png`
- **JSON**: `2025-11-11T14-30-45_reliability_test.json` (metadata)

### Exported Data

- **CSV**: `scattering_stats_2025-11-11T14-30-45.csv`
- **Heatmap PNG**: `2025-11-11T14-30-45_scattering_heatmap.png`

All files saved to `capture_directory` from INI file.

## Binary Image Mode

**What is it?**
Extracts specific bits from 8-bit camera image for reliability testing.

**Configuration**:
- `binary_bit_1 = 5` → Extracts bit 5 (value 32)
- `binary_bit_2 = 6` → Extracts bit 6 (value 64)

**Result**:
- Pixels with values 32, 64, or 96 appear white
- All other pixels appear black
- Creates binary representation for noise analysis

**Bit Position Values**:
- Bit 0 = 1, Bit 1 = 2, Bit 2 = 4, Bit 3 = 8
- Bit 4 = 16, Bit 5 = 32, Bit 6 = 64, Bit 7 = 128

## Tips for Non-Expert Users

1. **Start with Help**: Press F1 to see complete usage guide
2. **Use Tooltips**: Hover over any button for explanation
3. **Follow Workflow**: Save → Load → Compare or Analyze
4. **Monitor Statistics**: Check scattering % regularly
5. **Export Data**: Save CSV for later analysis
6. **Read INI Comments**: `event_config.ini` has detailed explanations
7. **Use Defaults**: Default settings work for most cases

## Advanced Configuration

### Low Noise Testing
```ini
accumulation_time_us = 33000
bias_hpf = 120
trail_filter_enabled = 1
```

### High Temporal Resolution
```ini
accumulation_time_us = 5000
bias_diff_on = 15
bias_diff_off = 15
```

### Stable Long-Term Monitoring
```ini
accumulation_time_us = 20000
bias_hpf = 100
erc_enabled = 1
```

## Technical Architecture

- **Language**: C++17
- **Build System**: CMake 3.26+
- **Graphics**: OpenGL 3.0, GLFW, GLEW, ImGui
- **Image Processing**: OpenCV 4.8
- **Event Camera**: Metavision SDK
- **Platform**: Windows (Visual Studio 2022)

## Project Structure

```
ReliabilityTesting/
├── start_reliability_camera.bat    # Launch script
├── event_config.ini                 # Configuration file
├── README.md                        # This file
├── PROJECT.md                       # Original specification
├── DEVELOPMENT_HISTORY.md           # Implementation details
├── CMakeLists.txt                   # Build configuration
├── include/                         # Header files
├── src/                             # Source files
├── build/                           # Build output
│   └── bin/Release/                 # Executable location
└── deps/                            # Dependencies
```

## Building from Source

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Run
build/bin/Release/reliability_testing_camera.exe
```

Or use Visual Studio 2022 to open CMakeLists.txt.

## Version History

- **v1.0** - Complete implementation (Phases 1-6)
  - Single camera support
  - Binary image processing
  - Image persistence with metadata
  - Three comparison modes
  - Scattering analysis with heatmaps
  - Data export (CSV, PNG)
  - User-friendly UI with help system

## Support & Documentation

- **Help Window**: Press F1 in application
- **Configuration Guide**: See comments in `event_config.ini`
- **Project Specification**: `PROJECT.md`
- **Development Details**: `DEVELOPMENT_HISTORY.md`

## License

Internal use for reliability testing of CenturyArks SilkyEvCam HD event cameras.

## Contact

For issues or questions, see application help (F1) or documentation files.

---

**Quick Reference Card**

| Action | How To |
|--------|--------|
| Start App | Double-click `start_reliability_camera.bat` |
| Get Help | Press **F1** |
| Save Image | Button → Add comment → Save |
| Load Image | Button → Enter path → Load |
| Compare | Load image → "Compare Images" button |
| Scattering | Load reference → "Start Scattering Analysis" |
| Export Stats | "Export Statistics (CSV)" |
| Export Heatmap | "Export Heatmap (PNG)" |
| Close App | Press **ESC** |
