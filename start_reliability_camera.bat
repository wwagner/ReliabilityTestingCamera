@echo off
REM ============================================================================
REM Reliability Testing Camera - Startup Script
REM ============================================================================
REM This script launches the Reliability Testing Camera application
REM Double-click this file to start the application
REM ============================================================================

echo.
echo ========================================
echo Reliability Testing Camera
echo ========================================
echo.

REM Get the directory where this batch file is located
set "SCRIPT_DIR=%~dp0"

REM Check if Release build exists
if exist "%SCRIPT_DIR%build\bin\Release\reliability_testing_camera.exe" (
    echo Starting application (Release build)...
    echo.
    echo Location: %SCRIPT_DIR%build\bin\Release\
    echo.

    REM Change to the Release directory (needed for DLLs and config files)
    cd /d "%SCRIPT_DIR%build\bin\Release"

    REM Launch the application
    start "" "reliability_testing_camera.exe"

    echo.
    echo Application launched!
    echo If the application window doesn't appear, check for error messages.
    echo.
    timeout /t 3 /nobreak >nul
    exit

) else if exist "%SCRIPT_DIR%build\bin\Debug\reliability_testing_camera.exe" (
    echo Starting application (Debug build)...
    echo.
    echo Location: %SCRIPT_DIR%build\bin\Debug\
    echo.

    REM Change to the Debug directory (needed for DLLs and config files)
    cd /d "%SCRIPT_DIR%build\bin\Debug"

    REM Launch the application
    start "" "reliability_testing_camera.exe"

    echo.
    echo Application launched!
    echo If the application window doesn't appear, check for error messages.
    echo.
    timeout /t 3 /nobreak >nul
    exit

) else (
    echo ERROR: Application executable not found!
    echo.
    echo Please build the project first using CMake:
    echo   1. Open CMake GUI or use command prompt
    echo   2. Run: cmake -B build -DCMAKE_BUILD_TYPE=Release
    echo   3. Run: cmake --build build --config Release
    echo.
    echo Expected location:
    echo   %SCRIPT_DIR%build\bin\Release\reliability_testing_camera.exe
    echo   or
    echo   %SCRIPT_DIR%build\bin\Debug\reliability_testing_camera.exe
    echo.
    echo Current directory: %CD%
    echo Script directory: %SCRIPT_DIR%
    echo.
    pause
)
