@echo off
title UnknownOS ISO Builder
color 0A

echo ========================================
echo   Unknown OS - ISO Builder
echo   Running in WSL...
echo ========================================
echo.

wsl --exec bash -c "set -e
sudo apt update 2>/dev/null
sudo apt install -y build-essential nasm xorriso qemu-system-x86 qemu-utils git gcc-multilib curl 2>/dev/null
git clone --depth=1 https://github.com/limine-bootloader/limine.git /tmp/limine 2>/dev/null || true
cd /tmp/limine && make 2>/dev/null

cd /mnt/c/Users/GODX/UnknownOS
if [ -f kernel/Makefile ]; then
    cd kernel && make clean && make -j\$(nproc)
fi

mkdir -p iso/boot initrd/bin
cp kernel/kernel.elf iso/boot/kernel.elf 2>/dev/null || true
echo '#!/bin/sh' > initrd/bin/init
echo 'export PATH=/bin:/sbin:/usr/bin:/usr/sbin' >> initrd/bin/init
echo 'exec /bin/sh' >> initrd/bin/init
cd initrd && tar -cf ../iso/boot/initrd.tar . && cd ..
mkdir -p iso/EFI/BOOT
cp /tmp/limine/BOOTX64.EFI iso/EFI/BOOT/ 2>/dev/null || true
cp /tmp/limine/limine-bios-cd.bin iso/ 2>/dev/null || true
cp /tmp/limine/limine-uefi-cd.bin iso/ 2>/dev/null || true
echo 'TIMEOUT=0' > iso/limine.cfg
cd iso
xorriso -as mkisofs -b limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin -o ../unknownos.iso . 2>/dev/null
cd ..
if [ -f unknownos.iso ]; then
    echo ISO built: unknownos.iso
    ls -lh unknownos.iso
fi
"

echo.
echo ========================================
echo   BUILD PROCESS COMPLETE
echo ========================================
echo.
pause
echo Checking for ISO...
dir unknownos.iso 2>nul
echo.
pause