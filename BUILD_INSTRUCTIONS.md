# Unknown OS - Build Instructions

**India's First Independent Operating System**  
Author: byrex <byrex.me@proton.me>

---

## Two Build Targets

| Target | Description | Requirements |
|--------|-------------|--------------|
| **Independent Kernel** | Custom kernel (no Linux), .unk format, Limine bootloader | Linux host with cross-compiler |
| **Linux-based (KDE Plasma)** | Arch Linux + KDE Plasma 6 + macOS theming | Arch Linux host or Docker |

---

## Option 1: Independent Kernel (Custom OS)

### Prerequisites (Linux)

```bash
# Ubuntu/Debian
sudo apt update && sudo apt install -y \
  x86_64-elf-gcc x86_64-elf-binutils \
  nasm xorriso make git qemu-system-x86 ovmf

# Arch Linux
sudo pacman -S x86_64-elf-gcc x86_64-elf-binutils nasm xorriso make git qemu-system-x86 ovmf

# Fedora
sudo dnf install x86_64-elf-gcc x86_64-elf-binutils nasm xorriso make git qemu-system-x86 ovmf
```

### Build

```bash
cd UnknownOS
chmod +x build_independent.sh
./build_independent.sh
```

This will:
1. Build the custom kernel (`kernel/kernel.elf`)
2. Create initrd with minimal userspace
3. Download Limine bootloader
4. Create bootable ISO (`unknownos.iso`) supporting both BIOS and UEFI

### Test in QEMU

```bash
# BIOS mode
qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk

# UEFI mode (requires OVMF)
qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk \
  -bios /usr/share/ovmf/OVMF.fd

# With KVM acceleration (Linux host)
qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk \
  -enable-kvm -cpu host -m 2G
```

### GitHub Actions (Zero Setup)

Push to GitHub and the workflow `.github/workflows/build-independent-kernel.yml` will:
- Build the kernel and ISO automatically
- Test boot in QEMU (BIOS and UEFI)
- Upload `unknownos.iso` as artifact

---

## Option 2: Linux-based (KDE Plasma Daily Driver)

### Prerequisites

- **Best**: Arch Linux host
- **Alternative**: Docker with `archlinux/archiso` image

### Build on Arch Linux

```bash
cd UnknownOS/iso-profile
sudo mkarchiso -v -w /tmp/build -o ../out .
```

### Build with Docker (any Linux)

```bash
cd UnknownOS
docker run --rm --privileged \
  -v "$(pwd)/iso-profile:/profile:ro" \
  -v "$(pwd)/out:/out" \
  archlinux/archiso:latest \
  mkarchiso -v -w /tmp/build -o /out /profile
```

### GitHub Actions

The workflow `.github/workflows/build-linux-based.yml` builds the KDE Plasma ISO automatically.

---

## exe2unk Converter (ELF → .unk)

### Build

```bash
cd UnknownOS/tools/exe2unk
make
```

### Usage

```bash
# Convert ELF to .unk
./exe2unk input.elf output.unk

# With capabilities
./exe2unk input.elf output.unk --cap fs_read --cap net_tcp_client

# With metadata
./exe2unk input.elf output.unk --metadata app.json

# Sign (requires Ed25519 key)
./exe2unk input.elf output.unk --sign --key-id 0x1234
```

### Test

```bash
# Create test ELF
cat > test.c << 'EOF'
int main() { return 42; }
EOF
x86_64-elf-gcc -nostdlib -ffreestanding -o test.elf test.c

# Convert
./exe2unk test.elf test.unk
file test.unk
```

---

## Architecture Overview

### Independent Kernel Components

```
kernel/
├── include/kernel.h          # Complete kernel API
├── src/kernel.c              # Entry point, init sequence
├── arch/x86_64/
│   ├── start.asm             # _start, Limine boot
│   ├── gdt.asm               # GDT with TSS
│   ├── idt.asm               # IDT (exceptions, IRQs, syscall)
│   ├── paging.asm            # Early VMM setup
│   ├── context.asm           # Context switching
│   ├── isr.c                 # Exception handlers
│   ├── irq.c                 # APIC-based IRQ handlers
│   └── apic.c                # APIC/IOAPIC driver + timer
├── mm/
│   ├── pmm.c                 # Bitmap physical memory manager
│   ├── vmm.c                 # 4-level page tables
│   └── heap.c                # Buddy allocator
├── sched/
│   ├── task.c                # Round-robin scheduler
│   └── syscalls.c            # Syscall handler + VFS fd_table
├── drivers/
│   ├── serial.c              # UART 16550
│   ├── vga.c                 # Limine framebuffer
│   ├── ps2.c                 # PS/2 keyboard
│   └── pci.c                 # PCI enumeration
├── fs/
│   ├── vfs.c                 # VFS with path traversal
│   ├── tarfs.c               # TAR (initrd) filesystem
│   ├── ramfs.c               # In-memory filesystem
│   └── unk_loader.c          # .unk binary loader
├── linker.ld                 # Higher-half (0xFFFF800000000000)
├── Makefile                  # Build system
└── specs/unk_format.md       # .unk format specification
```

### Key Features Implemented

| Subsystem | Status |
|-----------|--------|
| Limine boot protocol (BIOS/UEFI) | ✅ |
| Higher-half kernel (0xFFFF800000000000) | ✅ |
| 4-level paging + HHDM | ✅ |
| Bitmap PMM | ✅ |
| Buddy heap allocator | ✅ |
| GDT/TSS | ✅ |
| IDT (32 exceptions + 16 IRQs + APIC timer + syscall) | ✅ |
| APIC/IOAPIC with PIT calibration | ✅ |
| Round-robin scheduler + context switch | ✅ |
| Syscall interface (syscall/sysret) | ✅ |
| Per-task VFS file descriptor table | ✅ |
| VFS with mount management | ✅ |
| TARFS (initrd) + RAMFS | ✅ |
| .unk binary loader + relocation | ✅ |
| Capability-based security (12 caps) | ✅ |
| exe2unk converter | ✅ |
| UART + Framebuffer + PS/2 + PCI drivers | ✅ |
| Kernel shell with commands | ✅ |

---

## Developing on Windows

### Option 1: WSL2 (Recommended)

```powershell
# Install WSL2 with Ubuntu
wsl --install -d Ubuntu

# Then in WSL:
cd /mnt/c/Users/GODX/UnknownOS
./build_independent.sh
```

### Option 2: GitHub Actions (No local setup)

1. Push repository to GitHub
2. Go to Actions tab
3. Run "Build Unknown OS Independent Kernel" workflow
4. Download `unknownos.iso` artifact

### Option 3: Virtual Machine

- Install Ubuntu 24.04 in VirtualBox/VMware
- Follow Linux build instructions

---

## Project Structure

```
UnknownOS/
├── kernel/                     # Custom independent kernel
│   ├── include/kernel.h
│   ├── src/kernel.c
│   ├── arch/x86_64/
│   ├── mm/, sched/, drivers/, fs/
│   ├── Makefile, linker.ld
│   └── specs/unk_format.md
├── tools/exe2unk/              # ELF to .unk converter
├── build_independent.sh        # Build script for custom kernel
├── limine.cfg                  # Limine bootloader config
├── iso-profile/                # ArchISO profile (Linux-based)
├── .github/workflows/
│   ├── build-independent-kernel.yml
│   └── build-linux-based.yml
├── README.md
├── DEVELOPMENT_PLAN.md
└── BUILD_INSTRUCTIONS.md
```

---

## Next Development Steps

### Kernel (Priority Order)

1. **Complete syscall implementations**
   - `fork()` with copy-on-write
   - `execve()` with proper user mode transition
   - `mmap()` / `munmap()`
   - Signal handling

2. **Userspace libc**
   - Port musl libc to Unknown OS
   - System call wrappers
   - Standard library functions

3. **Init system & userspace**
   - Proper init (PID 1) with service management
   - Shell with job control
   - Core utilities (ls, cat, cp, mv, etc.)

4. **Device drivers**
   - AHCI/SATA driver
   - NVMe driver
   - VirtIO (for VMs)
   - GPU framebuffer (basic)
   - Network stack (virtio-net)

5. **Graphics subsystem**
   - Window manager / compositor
   - Desktop environment (dock, panel, etc.)

6. **Installer**
   - Partitioning tool
   - Dual-boot detection
   - Calamares-like installer

---

## Support

- **Email**: byrex.me@proton.me
- **OS Name**: Unknown OS
- **Tagline**: India's First Independent Operating System

---

## License

This project is developed for educational and research purposes.
See individual file headers for licensing information.