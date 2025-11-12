@echo off
REM Reliability Testing Camera Launcher
REM Sets plugin path BEFORE starting the application

echo Starting Reliability Testing Camera...
echo.

REM Get the directory where this batch file is located
set "SCRIPT_DIR=%~dp0"

REM Set plugin path to local plugins folder
set "MZ_HAL_PLUGIN_PATH=%SCRIPT_DIR%plugins"

echo Plugin path set to: %MZ_HAL_PLUGIN_PATH%
echo.

REM Launch the application
"%SCRIPT_DIR%reliability_testing_camera.exe"

REM Pause only if there was an error
if errorlevel 1 (
    echo.
    echo Application exited with error code: %errorlevel%
    pause
)
