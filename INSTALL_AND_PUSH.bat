@echo off
color 0A
title Install Git & Push UnknownOS

echo ========================================
echo  INSTALL GIT & PUSH TO GITHUB
echo  (One-click setup)
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
echo [1/4] Downloading Git for Windows...
curl -L -o "git-installer.exe" "https://github.com/git-for-windows/git/releases/download/v2.44.0.windows.1/Git-2.44.0-64-bit.exe"

echo.
echo [2/4] Installing Git (silent install)...
git-installer.exe /VERYSILENT /NORESTART

echo.
echo [3/4] Setting up git...
setx PATH "%PATH%;C:\Program Files\Git\cmd" /m
git config --global user.name "byrex"
git config --global user.email "byrex.me@proton.me"

echo.
echo [4/4] Pushing to GitHub...
cd /d "C:\Users\GODX\UnknownOS"
git init >nul 2>&1

echo.
set /p REPO="Enter your GitHub repo URL (https://github.com/username/unknownos): "
git remote add origin "%REPO%"
git add -A
git commit -m "Complete UnknownOS - 6 features"
git branch -M main
git push -u origin main

echo.
echo ========================================
echo  DONE! Check Actions tab for ISO build
echo ========================================
pause