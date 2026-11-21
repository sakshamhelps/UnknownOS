#!/bin/bash
# Unknown OS - Auto Build Script
# Run this in WSL after installation

set -e

echo "========================================"
echo "  Unknown OS - Auto Build"
echo "========================================"

# Install dependencies (Ubuntu/Debian)
echo "[1/4] Installing build tools..."
sudo apt update
sudo apt install -y build-essential nasm xorriso qemu-system-x86 qemu-utils git gcc-multilib

# Install Limine dependency
echo "[2/4] Setting up Limine bootloader..."
if [ ! -d limine ]; then
    git clone https://github.com/limine-bootloader/limine.git --branch=v7.x-branch-binary --depth=1
    cd limine
    make
    cd ..
fi

# Build kernel
echo "[3/4] Building kernel..."
cd kernel
make clean
make -j$(nproc)
cd ..

if [ ! -f kernel/kernel.elf ]; then
    echo "ERROR: Kernel build failed!"
    exit 1
fi

echo "Kernel built: $(ls -lh kernel/kernel.elf | awk '{print $5}')"

# Create ISO
echo "[4/4] Creating ISO..."
mkdir -p iso/boot

# Copy kernel
cp kernel/kernel.elf iso/boot/kernel.elf

# Create initrd
echo "Creating initrd..."
mkdir -p initrd/{bin,etc,dev,tmp,home,root,proc,sys,lib}

# Create terminal binary
cat > initrd/bin/terminal << 'EOF'
#!/bin/sh
echo "OS Terminal v1.0"
echo "Type 'help' for commands"
while true; do
    printf "(terminal) "
    read cmd
    case $cmd in
        help) echo "clear  exit  help  version" ;;
        clear) clear ;;
        version) echo "UnknownOS Terminal 1.0" ;;
        exit) break ;;
        *) echo "Unknown command: $cmd" ;;
    esac
done
EOF
chmod +x initrd/bin/terminal

# Set as init
cat > initrd/bin/init << 'EOF'
#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
echo "UnknownOS 1.0.0-IND"
echo "Type 'terminal' to start the terminal"
echo ""
exec /bin/sh
EOF
chmod +x initrd/bin/init

# Create initrd.tar
cd initrd
tar -cf ../iso/boot/initrd.tar .
cd ..

# Copy bootloader files
mkdir -p iso/EFI/BOOT
cp limine/BOOTX64.EFI iso/EFI/BOOT/
cp limine/BOOTIA32.EFI iso/EFI/BOOT/
cp limine/limine-bios.sys iso/
cp limine/limine-bios-cd.bin iso/
cp limine/limine-uefi-cd.bin iso/
cp limine.cfg iso/

# Build ISO
echo "Building ISO image..."
xorriso -as mkisofs \
    -b limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    -o ../unknownos.iso \
    .

# Make bootable for BIOS
cd iso
../limine/limine bios-install ../unknownos.iso
cd ..

echo ""
echo "========================================"
echo "  BUILD COMPLETE!"
echo "  ISO: unknownos.iso"
echo "========================================"
echo ""
echo "To test in QEMU:"
echo "  qemu-system-x86_64 -cdrom unknownos.iso -m 512 -boot d"