@echo off
REM ============================================================================
REM Reliability Testing Camera - Simple Launcher
REM ============================================================================
REM This script runs the camera application from the root folder
REM Uses event_config.ini from the current directory
REM ============================================================================

echo.
echo Starting Reliability Testing Camera...
echo.

REM Get the directory where this batch file is located
set "ROOT_DIR=%~dp0"

REM Set PATH to include the Release directory (for DLLs)
set "PATH=%ROOT_DIR%build\bin\Release;%PATH%"

REM Run the application from the root directory
REM This way it will use the root folder's event_config.ini
"%ROOT_DIR%build\bin\Release\reliability_testing_camera.exe"

echo.
echo Application closed.
pause
