@echo off
color 0A
title Enable WSL for UnknownOS Build

echo ========================================
echo  UNWSL Enable & Build Installer
echo ========================================
echo.

REM Must be run as Administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as Administrator - GREAT!
) else (
    echo ERROR: Must run as Administrator!
    echo Right-click this file > "Run as administrator"
    echo.
    pause
    exit /b 1
)

echo.
echo [1/4] Enabling WSL feature...
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart

echo.
echo [2/4] Enabling Virtual Machine Platform (for WSL2)...
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart

echo.
echo [3/4] Setting WSL2 as default...
wsl --set-default-version 2

echo.
echo [4/4] All done!
echo.
echo ========================================
echo  NEXT STEPS
echo ========================================
echo 1. RESTART YOUR COMPUTER
echo 2. Open 'Ubuntu' from Start Menu
echo 3. Complete Ubuntu first-run setup
echo 4. In Ubuntu terminal, run:
echo.
echo     cd /mnt/c/Users/GODX/UnknownOS
echo     sudo apt update
echo     sudo apt install -y build-essential nasm xorriso qemu-system-x86 qemu-utils git gcc-multilib
echo     chmod +x build_independent.sh 2>/dev/null
echo     ./build_independent.sh
echo.
echo ========================================
echo  DONE! Press any key to check status...
echo ========================================
pause

echo.
echo Checking WSL...
wsl --list --online 2>nul