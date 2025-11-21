# Reliability Testing Camera

A streamlined event camera viewer for reliability testing and noise evaluation of CenturyArks SilkyEvCam HD event cameras.

## Quick Start

1. **Start the Application**:
   - Double-click `start.bat` from the root folder
   - Or run: `build\bin\Release\reliability_testing_camera.exe`

2. **First Use**:
   - Press **F1** for help
   - Camera status shown in Status panel (green = connected)
   - Two windows: Status (left), Camera View (center)
   - Default configuration: bits 5 and 6 displayed
   - Configure settings in `event_config.ini` if needed

3. **Basic Workflow**:
   - View live camera feed
   - Load/save images for analysis
   - Run noise analysis on images
   - Adjust camera filters in real-time

## Overview

This application enables reliability testing with:
- Single camera viewer with integrated controls
- Real-time binary image processing
- Noise analysis with SNR/contrast measurements
- Filter controls (analog biases + trail filter)
- Image load/save with metadata

**Key Features**:
- **Binary image mode** - Configurable bit extraction (2 bits)
- **Real-time event rate chart** - Monitor event rates with configurable display
- **Noise analysis** - Detect signal dots and measure background noise
- **Filter controls** - Adjust camera biases and trail filter in real-time
- **Image management** - Load/save images with timestamps and metadata
- **Streamlined UI** - All controls in one viewer panel

## System Requirements

- **Hardware**: CenturyArks SilkyEvCam HD event camera
- **OS**: Windows 10/11
- **Dependencies**: Metavision SDK, OpenCV 4.8, OpenGL/GLFW

## Configuration

All settings are in `event_config.ini` (restart application after changes):

### Essential Settings

```ini
[Camera]
# Binary image bit positions
binary_bit_1 = 5                  # First bit (0-7) - Default: bit 5
binary_bit_2 = 6                  # Second bit (0-7) - Default: bit 6

# Frame rate
accumulation_time_us = 10000      # 10ms = ~100 FPS

# Analog biases
bias_diff = 0                     # Event detection threshold
bias_diff_on = 0                  # ON event threshold
bias_diff_off = 0                 # OFF event threshold
bias_hpf = 0                      # High-pass filter
bias_refr = 0                     # Refractory period

# Trail Filter
trail_filter_enabled = true       # Enable/disable trail filter
trail_filter_type = 2             # 0=TRAIL, 1=STC_CUT_TRAIL, 2=STC_KEEP_TRAIL
trail_filter_threshold = 1000     # Threshold in microseconds

# Storage
capture_directory = C:\Users\wolfw\OneDrive\Desktop\ReliabilityTesting
```

See `event_config.ini` for complete settings with detailed explanations.

## Features & Usage

### 1. Camera Viewer

The main Camera View window contains all controls:

**Mode Selector** (dropdown):
- **Active Camera**: View live camera feed
- **Load Image...**: Load a previously saved image
- **Save Image...**: Save current frame with metadata

**Image Display**:
- Shows binary processed camera feed or loaded image
- Automatically scales to fit window
- Maintains aspect ratio

### 2. Event Rate Chart (New!)

Real-time visualization of camera event rates displayed between the camera view and Image Analysis section.

**Chart Features**:
- **Rolling Time Window**: Shows event history over configurable time period
- **Auto-scaling Y-axis**: Automatically adjusts to data range
- **Color Coding**: Green line plot for easy visibility
- **Real-time Updates**: Updates at 10Hz for smooth visualization
- **Information Overlay**: Shows current rate, time range, and scale

**Default Display**:
- 60-second time window
- Auto-scaling enabled with 1 kev/s minimum
- Updates continuously when camera is active

### 3. Noise Analysis

Analyzes images to detect circular dots (signal) and measure background noise.

**Detection Parameters**:
- **Threshold**: Binary threshold for detecting bright dots (0-255)
- **Min/Max Dot Area**: Size range in pixels
- **Circularity**: Shape filter (0-1, where 1 = perfect circle)

**Running Analysis**:
1. Ensure image is loaded (camera or file)
2. Adjust detection parameters if needed
3. Click "Run Noise Analysis"

**Results Provided**:
- **Signal Statistics**: Mean, std dev, range, pixel count
- **Noise Statistics**: Mean, std dev, range, pixel count
- **Quality Metrics**:
  - SNR (dB) - Signal-to-Noise Ratio
  - Contrast Ratio
  - Color-coded: Green (excellent), Yellow (good), Red (poor)

**Visualization**:
- Detected Circles: Shows detected signal dots
- Signal Only: Shows signal mask
- Noise Only: Shows noise mask

**Export Results**:
- Click "Export Results" to save analysis to timestamped text file
- Format: `noise_analysis_YYYYMMDD_HHMMSS.txt`

### 4. Filters & Settings (Real-Time Control)

Control camera hardware settings without restarting:

**Analog Bias Filters**:
- **ON Event Threshold** (`bias_diff_on`): Controls brightness increase sensitivity
  - Lower = more ON events, Higher = fewer ON events
  - Range: -85 to 140
- **OFF Event Threshold** (`bias_diff_off`): Controls brightness decrease sensitivity
  - Lower = more OFF events, Higher = fewer OFF events
  - Range: -35 to 190
- **High-Pass Filter** (`bias_hpf`): Removes DC component from signal
  - Higher = stronger filtering, reduces background noise
  - Range: 0 to 120
- **Refractory Period** (`bias_refr`): Prevents rapid re-triggering
  - Higher = longer dead time, reduces noise but may miss rapid changes
  - Range: -20 to 235

**Trail Filter**:
- **Enable/Disable**: Toggle trail filtering
- **Filter Type**:
  - TRAIL: Basic filtering
  - STC_CUT_TRAIL: Cut trailing events
  - STC_KEEP_TRAIL: Keep stable events (recommended)
- **Threshold (μs)**: Events older than this are filtered
  - Range: 1,000 to 100,000 microseconds

**Chart Settings** (New!):
Configure the event rate chart display:

- **Time Window**: Adjust history from 10 to 600 seconds
  - Default: 60 seconds
  - Longer windows show trends over time
- **Autoscale**: Enable/disable automatic Y-axis scaling
  - Default: Enabled
  - When enabled: Chart adjusts to fit data with 20% headroom
  - When disabled: Uses fixed maximum scale
- **Minimum Scale**: Set minimum events/second (0.1-9999 kev/s)
  - Default: 1 kev/s
  - Lower values zoom in on low event rates
- **Maximum Scale**: Set maximum events/second (1-10000 kev/s)
  - Default: 1000 kev/s
  - Used as fixed scale when autoscale is disabled
- **Reset to Defaults**: Restore all chart settings

**Applying Changes**:
- Adjust sliders/settings
- Click "Apply Bias Changes" or "Apply Trail Filter" button
- Changes take effect immediately on camera hardware
- Chart settings update instantly - no apply button needed
- No restart required!

### 5. Image Management

**Save Image**:
- Captures current camera frame with timestamp
- Stores metadata (camera settings, user comments)
- Format: `YYYY-MM-DDTHH-MM-SS_reliability_test.png` + `.json`
- Saved to directory configured in `event_config.ini`

**Load Image**:
- Opens Windows file browser
- Supports PNG files
- Automatically loads metadata if available
- Use for offline analysis

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **F1** | Toggle Help window |
| **ESC** | Close application |

## Common Use Cases

### Noise Characterization

Quantify sensor noise with varying filter settings:
1. Capture dark frame (lens cap on)
2. Load the dark frame
3. Run noise analysis
4. Adjust filter settings in Filters panel
5. Click "Apply" to test new settings
6. Compare SNR values
7. Export results for documentation

### Filter Optimization

Find optimal bias settings for your environment:
1. View live camera feed
2. Open Filters section
3. Monitor event rate chart while adjusting
4. Adjust bias sliders while watching live view
5. Click "Apply Bias Changes" to test
6. Watch event rate chart for immediate feedback
7. Run noise analysis to measure impact
8. Iterate until SNR is maximized
9. Update `event_config.ini` with final values

### Focus Adjustment with Event Rate Monitoring

Use the event rate chart for real-time focus tuning:
1. Open Camera Settings → Chart Settings
2. Set time window to 30 seconds for quick response
3. Disable autoscale and set max to expected rate
4. Adjust camera focus while monitoring chart
5. Optimal focus shows stable, high event rate
6. Poor focus shows low or erratic event rates

### Long-Term Reliability Testing

Monitor camera performance over time:
1. Capture baseline image with optimal settings
2. Save baseline for future comparison
3. Periodically capture new images
4. Load and analyze each image
5. Compare SNR trends over time
6. Export all results for reporting

### Signal Detection Tuning

Optimize detection parameters for your test pattern:
1. Load test image with known signal dots
2. Adjust detection parameters
3. Run noise analysis
4. Check visualization to verify detection
5. Refine parameters until accurate
6. Use same parameters for all test images

## Troubleshooting

### Camera Not Connected

**Symptoms**: Status shows "Disconnected" (red)

**Solutions**:
- Check USB connection
- Verify camera is powered
- Restart application
- Check Metavision SDK installation

### Filters Grayed Out

**Symptoms**: Filter controls disabled

**Solutions**:
- Filters require camera to be connected
- Check camera status in Status panel
- Reconnect camera if needed

### High Noise in Images

**Symptoms**: Low SNR, high noise statistics

**Solutions**:
- Adjust High-Pass Filter (bias_hpf) upward
- Enable Trail Filter
- Increase Refractory Period (bias_refr)
- Check for environmental noise sources
- Use longer accumulation time

### No Signal Dots Detected

**Symptoms**: Zero dots detected in noise analysis

**Solutions**:
- Lower threshold value
- Decrease minimum area
- Reduce circularity threshold
- Check that image contains signal
- Verify binary bits extract correct data

## Understanding Statistics

### Noise Analysis Metrics

- **Detected Dots**: Number of circular signal regions found
- **Signal Mean/Std Dev**: Brightness statistics within detected dots
- **Noise Mean/Std Dev**: Brightness statistics in background
- **SNR (dB)**: Signal-to-Noise Ratio
  - > 40 dB = Excellent
  - 20-40 dB = Good
  - < 20 dB = Poor
- **Contrast Ratio**: Signal brightness / noise brightness

### Filter Effects

- **Higher bias_diff_on/off**: Fewer events, less noise, may miss weak signals
- **Lower bias_diff_on/off**: More events, captures weaker signals, more noise
- **Higher bias_hpf**: Removes slow temporal changes, reduces background
- **Higher bias_refr**: Reduces pixel re-triggering, less noise but slower response
- **Trail Filter**: Removes transient events and flickering

## Binary Image Mode

**What is it?**
Extracts specific bits from 8-bit camera image for reliability testing.

**Configuration**:
- `binary_bit_1 = 5` → Extracts bit 5 (value 32)
- `binary_bit_2 = 6` → Extracts bit 6 (value 64)

**Result**:
- Pixels with bit 5 OR bit 6 set appear white
- All other pixels appear black
- Creates binary representation for noise analysis

**Bit Position Values**:
- Bit 0 = 1, Bit 1 = 2, Bit 2 = 4, Bit 3 = 8
- Bit 4 = 16, Bit 5 = 32, Bit 6 = 64, Bit 7 = 128

## Tips for Users

1. **Start with Help**: Press F1 to see complete usage guide
2. **Use Tooltips**: Hover over controls for explanations
3. **Test Filter Changes**: Use "Apply" buttons to test settings live
4. **Monitor SNR**: Higher SNR = better signal quality
5. **Export Results**: Save analysis to text files for documentation
6. **Adjust Detection**: Tune parameters to match your test pattern
7. **Use Defaults**: Default settings work for most cases

## File Formats

### Saved Images

- **PNG**: `2025-11-11T14-30-45_reliability_test.png`
- **JSON**: `2025-11-11T14-30-45_reliability_test.json` (metadata)

### Exported Data

- **Noise Analysis**: `noise_analysis_YYYYMMDD_HHMMSS.txt`

All files saved to `capture_directory` from INI file.

## Technical Architecture

**Simplified Single-Camera Design**:
- Streamlined codebase (~7,500 lines vs. 15,000 originally)
- Single integrated viewer panel
- Real-time filter control
- Efficient binary image processing

**Technology Stack**:
- **Language**: C++17
- **Build System**: CMake 3.26+
- **Graphics**: OpenGL 3.0, GLFW, GLEW, ImGui
- **Image Processing**: OpenCV 4.8
- **Event Camera**: Metavision SDK
- **Platform**: Windows (Visual Studio 2022)

## Project Structure

```
ReliabilityTesting/
├── start.bat                        # Launch script for development
├── event_config.ini                 # Configuration file
├── README.md                        # This file
├── CMakeLists.txt                   # Build configuration
├── .gitignore                       # Git ignore file (includes Deployment/)
├── include/                         # Header files
│   ├── core/                        # Application state
│   ├── video/                       # Frame/texture management
│   └── ui/                          # User interface
│       ├── viewer_panel.h           # Main viewer panel
│       ├── event_rate_chart.h       # Event rate chart (NEW)
│       └── image_dialog.h           # Image dialogs
├── src/                             # Source files
│   ├── main.cpp                     # Application entry
│   ├── core/                        # Core modules
│   ├── video/                       # Video processing
│   └── ui/                          # UI implementation
│       ├── viewer_panel.cpp         # Viewer implementation
│       ├── event_rate_chart.cpp     # Chart implementation (NEW)
│       └── image_dialog.cpp         # Dialog implementation
├── build/                           # Build output
│   └── bin/Release/                 # Executable location
├── Deployment/                      # Deployment package (git-ignored)
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

- **v2.1** - Event Rate Monitoring (Current)
  - Added real-time event rate chart with 60-second history
  - Configurable chart settings (time window, scaling, min/max)
  - Auto-scaling with manual override option
  - Chart displays between camera view and analysis section
  - Deployment folder separated from development builds

- **v2.0** - Streamlined architecture
  - Single camera viewer with integrated controls
  - Real-time filter adjustment
  - Noise analysis built-in
  - 50% code reduction
  - Simplified, maintainable design

- **v1.0** - Initial implementation
  - Dual viewer support
  - Image comparison modes
  - Scattering analysis
  - Full feature set

## Support & Documentation

- **Help Window**: Press F1 in application
- **Configuration Guide**: See comments in `event_config.ini`
- **Tooltips**: Hover over any control for context help

## License

Internal use for reliability testing of CenturyArks SilkyEvCam HD event cameras.

---

**Quick Reference Card**

| Action | How To |
|--------|--------|
| Start App | Double-click `start_reliability_camera.bat` |
| Get Help | Press **F1** |
| View Live | Select "Active Camera" in dropdown |
| Save Image | Select "Save Image..." in dropdown |
| Load Image | Select "Load Image..." in dropdown |
| Run Analysis | Open "Noise Analysis" → Adjust params → "Run Noise Analysis" |
| Adjust Filters | Open "Filters" → Adjust sliders → Click "Apply" |
| Export Results | In "Noise Analysis" → "Export Results" |
| Close App | Press **ESC** |
