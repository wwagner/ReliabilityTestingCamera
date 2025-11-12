# Deployment Guide

## Quick Start

1. Run `create_deployment.ps1` to create deployment package:
   ```powershell
   powershell -ExecutionPolicy Bypass -File create_deployment.ps1
   ```

2. The `Deployment` folder will contain everything needed

3. ZIP the Deployment folder for distribution

4. Users should run `START_CAMERA.bat` to launch the application

## What's Included

The deployment package contains:
- `reliability_testing_camera.exe` - Main application
- `START_CAMERA.bat` - Recommended launcher (sets plugin path)
- `plugins/silky_common_plugin.dll` - HAL plugin for event camera
- All necessary Release DLL dependencies
- `event_config.ini` - Configuration file
- Documentation files

## User Requirements

Target systems need:
- Windows 10/11 (64-bit)
- Visual C++ Redistributable 2022 (x64): https://aka.ms/vs/17/release/vc_redist.x64.exe
- Event camera hardware (optional - runs in simulation mode without camera)

## Important Notes

### Plugin Path Issue

The Metavision HAL library searches for plugins in this order:
1. `MZ_HAL_PLUGIN_PATH` environment variable
2. System installation (`C:\Program Files\CenturyArks\plugins`)
3. Local `plugins/` folder (low priority)

**Solution**: The `START_CAMERA.bat` launcher sets `MZ_HAL_PLUGIN_PATH` before starting the application, ensuring the local plugin is used.

### Why START_CAMERA.bat?

Setting the environment variable in code (`_putenv_s` in main.cpp) happens **after** the HAL library loads and caches plugin paths. The batch file sets it **before** the process starts.

## Deployment Methods

### Method 1: Launcher (Recommended)
Users double-click `START_CAMERA.bat`
- ✓ Self-contained
- ✓ No system dependencies
- ✓ Portable

### Method 2: System Plugin Folder
Copy `plugins/silky_common_plugin.dll` to `C:\Program Files\CenturyArks\plugins\`
- Run `reliability_testing_camera.exe` directly
- Requires admin rights to copy to Program Files

### Method 3: With Metavision SDK Installed
If users have Metavision SDK already installed:
- Run `reliability_testing_camera.exe` directly
- Uses system plugins automatically

## Building Deployment Package

The `create_deployment.ps1` script:
1. Copies executable from `build/bin/Release`
2. Copies only Release DLLs (excludes `*_d.dll` debug versions)
3. Copies `plugins/` folder with `silky_common_plugin.dll`
4. Copies configuration and documentation files
5. Verifies no debug DLLs are present

## Troubleshooting

### "procedure entry point" DLL errors
- Ensure Visual C++ Redistributable 2022 (x64) is installed
- Use START_CAMERA.bat launcher
- Verify all files extracted from ZIP

### "[HAL][WARNING] no plugin found"
- Use START_CAMERA.bat launcher
- OR copy plugin to system folder

### Camera not detected
- Check Device Manager for camera
- Try different USB port
- Application runs in simulation mode without camera

## File Structure

```
Deployment/
├── reliability_testing_camera.exe
├── START_CAMERA.bat ← USE THIS
├── plugins/
│   └── silky_common_plugin.dll
├── *.dll (all dependencies)
├── event_config.ini
├── DEPLOYMENT_INSTRUCTIONS.txt
└── [documentation files]
```

## Updates

To update the deployment package:
1. Make code changes
2. Build in Release mode: `cmake --build build --config Release`
3. Run `create_deployment.ps1`
4. ZIP and distribute

## Technical Details

- Architecture: x64 (64-bit)
- Build Configuration: Release
- All DLLs are Release builds (no debug symbols)
- Self-contained (no installation required)
- Portable (can run from any folder)
