# Create Deployment Package for Reliability Testing Camera
# This script creates a clean deployment package without debug DLLs

param(
    [string]$BuildDir = "C:\Users\wolfw\source\repos\ReliabilityTesting\build\bin\Release",
    [string]$DeployDir = "C:\Users\wolfw\source\repos\ReliabilityTesting\Deployment"
)

Write-Host "Creating deployment package..." -ForegroundColor Green

# Create deployment directory if it doesn't exist
if (-not (Test-Path $DeployDir)) {
    New-Item -ItemType Directory -Path $DeployDir | Out-Null
    Write-Host "Created deployment directory: $DeployDir"
} else {
    Write-Host "Cleaning existing deployment directory..."
    Remove-Item "$DeployDir\*" -Recurse -Force -ErrorAction SilentlyContinue
}

# Copy main executable
Write-Host "Copying executable..."
Copy-Item "$BuildDir\reliability_testing_camera.exe" -Destination $DeployDir

# Copy all DLLs EXCEPT debug versions (*_d.dll)
Write-Host "Copying release DLLs only (excluding debug versions)..."
Get-ChildItem "$BuildDir\*.dll" | Where-Object { $_.Name -notmatch '_d\.dll$' } | Copy-Item -Destination $DeployDir

# Copy plugins directory
Write-Host "Copying plugins..."
if (Test-Path "$BuildDir\plugins") {
    Copy-Item "$BuildDir\plugins" -Destination $DeployDir -Recurse
}

# Copy configuration file
Write-Host "Copying configuration file..."
if (Test-Path "$BuildDir\event_config.ini") {
    Copy-Item "$BuildDir\event_config.ini" -Destination $DeployDir
}

# Copy documentation files from project root
$ProjectRoot = Split-Path -Parent $BuildDir | Split-Path -Parent | Split-Path -Parent
Write-Host "Copying documentation..."
if (Test-Path "$ProjectRoot\README.md") {
    Copy-Item "$ProjectRoot\README.md" -Destination $DeployDir
}
if (Test-Path "$ProjectRoot\QUICKSTART.txt") {
    Copy-Item "$ProjectRoot\QUICKSTART.txt" -Destination $DeployDir
}
if (Test-Path "$ProjectRoot\PACKAGE_CONTENTS.txt") {
    Copy-Item "$ProjectRoot\PACKAGE_CONTENTS.txt" -Destination $DeployDir
}

# Create START_HERE.bat if it doesn't exist
$startScript = @"
@echo off
echo Starting Reliability Testing Camera Application...
echo.
start reliability_testing_camera.exe
"@
$startScript | Out-File -FilePath "$DeployDir\START_HERE.bat" -Encoding ASCII

# Get file count
$fileCount = (Get-ChildItem $DeployDir -Recurse -File).Count
$totalSize = (Get-ChildItem $DeployDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB

Write-Host "`nDeployment package created successfully!" -ForegroundColor Green
Write-Host "Location: $DeployDir"
Write-Host "Files: $fileCount"
Write-Host "Total size: $([math]::Round($totalSize, 2)) MB"

# Verify no debug DLLs were copied
$debugDlls = Get-ChildItem "$DeployDir\*_d.dll" -ErrorAction SilentlyContinue
if ($debugDlls) {
    Write-Host "`nWARNING: Found debug DLLs in deployment:" -ForegroundColor Yellow
    $debugDlls | ForEach-Object { Write-Host "  - $($_.Name)" -ForegroundColor Yellow }
} else {
    Write-Host "`nVerified: No debug DLLs in deployment package" -ForegroundColor Green
}

Write-Host "`nNext steps:"
Write-Host "1. Test the application locally from: $DeployDir"
Write-Host "2. Create a ZIP file for distribution"
Write-Host "3. Ensure target machines have Visual C++ Redistributable 2022 (x64) installed"
Write-Host "   Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe"
