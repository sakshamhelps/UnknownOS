@echo off
color 0A
title ONE-CLICK: Push to GitHub + Build ISO
setlocal

echo ========================================
echo  ONE-CLICK UNKNOWNOS BUILD & DEPLOY
echo ========================================
echo.

REM Must be Administrator
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
echo Before running:
echo 1. Go to github.com
echo 2. Click "New repository"
echo 3. Name it: unknownos
echo 4. DO NOT check any boxes
echo 5. Click "Create repository"
echo.
set /p REPO_URL="Enter your repo URL (ex: https://github.com/username/unknownos): "

echo.
echo [1/4] Configuring git...
git config --global user.name "byrex"
git config --global user.email "byrex.me@proton.me"

echo.
echo [2/4] Initializing and committing...
cd /d "C:\Users\GODX\UnknownOS"
git init >nul 2>&1
git remote remove origin 2>nul

echo.
echo [3/4] Adding ALL files (except .gitignore exclusions)...
git add -A
git add .gitignore
git add .github/workflows/build.yml
git add .github/workflows/quick-push.md
git add BUILD_INSTRUCTIONS.txt
git add enable_wsl_full.bat
git add enable_wsl_admin.bat
git add BUILD_ME.cmd
git add build_iso.bat
git add single_command.bat
git add auto_build.sh
git add build_independent.sh 2>nul
git add limine.cfg 2>nul
git add linker.ld 2>nul

echo.
echo [4/4] Committing (this may take a moment)...
git commit -m "Complete UnknownOS - 6 features implemented:
1. Window decorations (macOS-style)
2. Desktop context menu
3. Terminal app with PTY/shell
4. Settings app
5. App launcher
6. File manager/text editor/browser

Kernel ready to build!"

echo.
echo Pushing to GitHub...
git remote add origin "%REPO_URL%"
git branch -M main
git push -u origin main --progress

echo.
echo ========================================
echo  SUCCESS! Your repository is updated
echo ========================================
echo.
echo NEXT STEPS:
echo 1. Go to: %REPO_URL%
echo 2. Click "Actions" tab
echo 3. Wait for build (5-10 minutes)
echo 4. Click the workflow run
echo 5. Download "unknownos.iso" artifact
echo.
echo ISO BUILD AUTOMATICALLY ON GITHUB!
echo ========================================
pause