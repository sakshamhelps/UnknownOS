#!/bin/bash
# Unknown OS - Build Script
# Author: byrex <byrex.me@proton.me>

set -e

echo "========================================"
echo "  Building Unknown OS Kernel"
echo "  India's First Independent OS"
echo "  byrex.me@proton.me"
echo "========================================"

# Check for cross-compiler
if ! command -v x86_64-elf-gcc &> /dev/null; then
    echo "ERROR: x86_64-elf-gcc not found in PATH"
    echo "Install the cross-compiler toolchain first"
    exit 1
fi

if ! command -v nasm &> /dev/null; then
    echo "ERROR: nasm not found in PATH"
    exit 1
fi

# Build kernel
cd kernel
make clean
make -j$(nproc)

if [ ! -f kernel.elf ]; then
    echo "ERROR: Kernel build failed"
    exit 1
fi

echo "Kernel built successfully: kernel/$(ls -lh kernel.elf | awk '{print $5}')"

# Copy to ISO directory
cd ..
mkdir -p iso/boot
cp kernel/kernel.elf iso/boot/kernel.elf

# Build initrd
echo "Building initrd..."
mkdir -p initrd/{bin,etc,dev,tmp,home,root,proc,sys}

# Copy any userspace programs here
# For now, create a minimal init script
cat > initrd/init << 'EOF'
#!/bin/sh
# Unknown OS - Init Script

export PATH=/bin:/sbin:/usr/bin:/usr/sbin

# Mount virtual filesystems
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

# Set hostname
echo "unknownos" > /proc/sys/kernel/hostname

# Start shell
exec /bin/sh
EOF
chmod +x initrd/init

# Create initrd.tar
cd initrd
tar -cf ../iso/boot/initrd.tar .
cd ..

echo "Initrd built: iso/boot/$(ls -lh iso/boot/initrd.tar | awk '{print $5}')"

# Build ISO with Limine
echo "Building ISO image..."

# Download Limine if not present
if [ ! -d limine ]; then
    echo "Downloading Limine..."
    git clone https://github.com/limine-bootloader/limine.git --branch=v7.x-branch-binary --depth=1
    cd limine
    make
    cd ..
fi

# Create ISO directory structure
mkdir -p iso/EFI/BOOT
cp limine/BOOTX64.EFI iso/EFI/BOOT/
cp limine/BOOTIA32.EFI iso/EFI/BOOT/
cp limine/limine-bios.sys iso/
cp limine/limine-bios-cd.bin iso/
cp limine/limine-uefi-cd.bin iso/
cp limine.cfg iso/

# Create ISO
xorriso -as mkisofs \
    -b limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    -o unknownos.iso \
    iso/

# Make ISO bootable for BIOS
limine/limine bios-install unknownos.iso

echo "========================================"
echo "  Build Complete!"
echo "  ISO: unknownos.iso ($(ls -lh unknownos.iso | awk '{print $5}'))"
echo "========================================"
echo ""
echo "To test in QEMU:"
echo "  qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk"
echo ""
echo "To test with KVM acceleration:"
echo "  qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk -enable-kvm -cpu host -m 2G"