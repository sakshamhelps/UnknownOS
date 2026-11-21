@echo off
echo Enabling WSL and installing Ubuntu...
echo.

REM Must run as Administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as Administrator - continuing...
) else (
    echo ERROR: This script must be run as Administrator
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Enable WSL feature
echo Enabling WSL...
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart

echo.
echo Installing Ubuntu via WSL...
wsl --install -d Ubuntu --no-launch

echo.
echo WSL installation initiated. Please:
echo 1. Wait for Ubuntu to download and install
echo 2. Run this script again to build
pause