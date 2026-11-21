#!/bin/bash
# Unknown OS - Independent Kernel Build Script
# Builds a fully independent OS with custom kernel (no Linux)
# Author: byrex <byrex.me@proton.me>

set -e

echo "========================================"
echo "  Building Unknown OS (Independent Kernel)"
echo "  India's First Independent OS"
echo "  byrex.me@proton.me"
echo "========================================"

# Check for cross-compiler
if ! command -v x86_64-elf-gcc &> /dev/null; then
    echo "ERROR: x86_64-elf-gcc not found in PATH"
    echo "Install the cross-compiler toolchain first:"
    echo "  On Ubuntu/Debian: apt install gcc-x86-64-elf binutils-x86-64-elf"
    echo "  On Arch: pacman -S x86_64-elf-gcc x86_64-elf-binutils"
    echo "  On Fedora: dnf install gcc-x86-64-elf binutils-x86-64-elf"
    exit 1
fi

if ! command -v nasm &> /dev/null; then
    echo "ERROR: nasm not found in PATH"
    exit 1
fi

if ! command -v xorriso &> /dev/null; then
    echo "ERROR: xorriso not found in PATH"
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

# Build initrd with minimal userspace
echo "Building initrd..."
mkdir -p initrd/{bin,etc,dev,tmp,home,root,proc,sys,lib}

# Create a simple init program (statically linked)
cat > initrd/init.c << 'EOF'
/* Minimal init for Unknown OS */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>

int main() {
    printf("Unknown OS Init starting...\n");

    /* Mount virtual filesystems */
    mkdir("/proc", 0555);
    mkdir("/sys", 0555);
    mkdir("/dev", 0755);

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    printf("Virtual filesystems mounted\n");

    /* Set hostname */
    FILE *f = fopen("/proc/sys/kernel/hostname", "w");
    if (f) {
        fprintf(f, "unknownos");
        fclose(f);
    }

    /* Start shell */
    printf("Starting shell...\n");
    execl("/bin/sh", "sh", NULL);

    /* If shell fails, drop to basic prompt */
    while (1) {
        printf("unknownos# ");
        char cmd[256];
        if (fgets(cmd, sizeof(cmd), stdin)) {
            system(cmd);
        }
    }

    return 0;
}
EOF

# Try to compile init statically
if command -v x86_64-elf-gcc &> /dev/null; then
    x86_64-elf-gcc -static -nostdlib -ffreestanding \
        -o initrd/bin/init initrd/init.c 2>/dev/null || {
        echo "Warning: Could not compile static init, using script fallback"
        cat > initrd/init << 'SCRIPTEOF'
#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin

# Mount virtual filesystems
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
mount -t devtmpfs devtmpfs /dev 2>/dev/null

echo "unknownos" > /proc/sys/kernel/hostname 2>/dev/null

# Start shell
exec /bin/sh
SCRIPTEOF
        chmod +x initrd/init
    }
fi

# Create a minimal shell script for testing
cat > initrd/bin/sh << 'EOF'
#!/bin/sh
# Minimal shell for Unknown OS
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PS1="unknownos> "

echo "Unknown OS Shell 1.0"
echo "Type 'help' for commands"

while true; do
    printf "$PS1"
    read cmd args
    case $cmd in
        help)
            echo "Commands:"
            echo "  help       - Show this help"
            echo "  clear      - Clear screen"
            echo "  reboot     - Reboot system"
            echo "  shutdown   - Power off (QEMU only)"
            echo "  ls         - List files"
            echo "  cat        - Show file contents"
            echo "  echo       - Print arguments"
            echo "  unkrun     - Run .unk binary"
            ;;
        clear) clear ;;
        reboot) echo "Rebooting..."; reboot -f ;;
        shutdown) echo "Shutting down..."; poweroff -f ;;
        ls) ls $args ;;
        cat) cat $args ;;
        echo) echo $args ;;
        unkrun) /bin/unkrun $args ;;
        "") ;;
        *) echo "Unknown command: $cmd. Type 'help'." ;;
    esac
done
EOF
chmod +x initrd/bin/sh

# Create unkrun tool
cat > initrd/bin/unkrun << 'EOF'
#!/bin/sh
# Run .unk binary
if [ $# -eq 0 ]; then
    echo "Usage: unkrun <program.unk> [args...]"
    exit 1
fi
exec "$@"
EOF
chmod +x initrd/bin/unkrun

# Create initrd.tar
cd initrd
tar -cf ../iso/boot/initrd.tar .
cd ..

echo "Initrd built: iso/boot/$(ls -lh iso/boot/initrd.tar | awk '{print $5}')"

# Download Limine if not present
if [ ! -d limine ]; then
    echo "Downloading Limine bootloader..."
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
echo "Building ISO image..."
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
echo "To test in QEMU (BIOS):"
echo "  qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk"
echo ""
echo "To test in QEMU (UEFI) - requires OVMF:"
echo "  qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk \\"
echo "    -bios /usr/share/ovmf/OVMF.fd"
echo ""
echo "To test with KVM acceleration:"
echo "  qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk -enable-kvm -cpu host -m 2G"