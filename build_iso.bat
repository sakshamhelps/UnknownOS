@echo off
REM Unknown OS ISO Builder - Windows Batch Script
REM Run this from Command Prompt as Administrator

echo ========================================
echo   Unknown OS - ISO Builder
echo ========================================

REM Check for WSL
echo [1/4] Checking WSL...
wsl --list --online 2>nul >nul
if errorlevel 1 (
    echo ERROR: WSL not installed or not working
    echo Run: wsl --install -d Ubuntu
    pause
    exit /b 1
)

echo WSL is available

REM Copy and run build script in WSL
echo [2/4] Setting up build environment in WSL...
wsl bash -c "cd /mnt/c/Users/GODX/UnknownOS && if [ ! -f build.sh ]; then cat > /tmp/auto_build.sh << 'SCRIPT'
#!/bin/bash
set -e
echo '[3/4] Installing build tools...'
sudo apt update -qq
sudo apt install -y -qq build-essential nasm xorriso qemu-system-x86 qemu-utils git gcc-multilib

echo '[4/4] Downloading Limine...'
if [ ! -d limine ]; then
    git clone --depth=1 https://github.com/limine-bootloader/limine.git /tmp/limine
fi
cd /tmp/limine && make

echo 'Building kernel...'
cd /mnt/c/Users/GODX/UnknownOS
if [ -f kernel/Makefile ]; then
    cd kernel && make clean && make -j\$(nproc)
fi

echo 'Creating ISO...'
mkdir -p iso/boot
if [ -f kernel/kernel.elf ]; then
    cp kernel/kernel.elf iso/boot/kernel.elf
fi

mkdir -p initrd/{bin,etc,dev,tmp,home,root,proc,sys,lib}
cp iso/boot/kernel.elf initrd/bin/ 2>nul || true

echo '#!/bin/sh' > initrd/bin/init
echo 'export PATH=/bin:/sbin:/usr/bin:/usr/sbin' >> initrd/bin/init
echo 'exec /bin/sh' >> initrd/bin/init

cd initrd && tar -cf ../iso/boot/initrd.tar . && cd ..

mkdir -p iso/EFI/BOOT
cp /tmp/limine/BOOTX64.EFI iso/EFI/BOOT/
cp /tmp/limine/limine-bios-cd.bin iso/
cp /tmp/limine/limine-uefi-cd.bin iso/
cp /tmp/limine/limine.cfg iso/ 2>nul || echo 'TIMEOUT=0' > iso/limine.cfg

echo 'Building ISO image...'
xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -o unknownos.iso iso/

if [ -f unknownos.iso ]; then
    cp unknownos.iso ..\\
    echo '========================================'
    echo '  BUILD SUCCESSFUL!'
    echo '  ISO: ..\\unknownos.iso'
    echo '========================================'
else
    echo 'ERROR: Build failed'
fi
SCRIPT
fi
chmod +x /tmp/auto_build.sh
wsl /tmp/auto_build.sh
"

echo.
echo Done. Check for unknownos.iso in the UnknownOS folder.