@echo off
color 0A
title Push UnknownOS to GitHub

echo ========================================
echo  PUSH UnknownOS TO GITHUB
echo  (GitHub Actions will build ISO for you)
echo ========================================
echo.

REM Must be Administrator for git
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as Administrator - GREAT!
) else (
    echo ERROR: Must run as Administrator!
    echo Right-click > "Run as administrator"
    echo.
    pause
    exit /b 1
)

echo.
echo Before running this script:
echo 1. Go to github.com
echo 2. Create new repository named "unknownos"
echo 3. Get your GitHub username ready
echo.
set /p USERNAME="Enter your GitHub username: "

echo.
echo [1/4] Configuring git...
git config --global user.name "%USERNAME%"
git config --global user.email "%USERNAME%@users.noreply.github.com"

echo.
echo [2/4] Initializing repository...
cd /d "C:\Users\GODX\UnknownOS"
git init

echo.
echo [3/4] Adding and committing files...
git add .
git commit -m "Initial commit: Unknown OS with all 6 features

Features:
- Window decorations (macOS-style)
- Desktop context menu
- Terminal app with PTY/shell
- Settings app
- App launcher
- File manager/text editor/browser"

echo.
echo [4/4] Creating remote...
git remote add origin https://github.com/%USERNAME%/unknownos.git

echo.
echo Pushing to GitHub...
git branch -M main
git push -u origin main

echo.
echo ========================================
echo  DONE! Check your repo on GitHub
echo ========================================
echo  1. Go to: https://github.com/%USERNAME%/unknownos
echo  2. Click "Actions" tab
echo  3. Wait for build (5-10 minutes)
echo  4. Click "Build UnknownOS ISO"
echo  5. Download ISO from Artifacts
echo.
echo  ISO will be built ON GITHUB AUTOMATICALLY!
echo ========================================
pause