# Unknown OS - Project Status

**Last Updated**: 2026-11-21  
**Version**: 1.0.0-IND (Independent Kernel)

---

## ✅ COMPLETED: Independent Kernel (Custom, Non-Linux)

### Core Kernel Subsystems

| Subsystem | File | Status |
|-----------|------|--------|
| **Boot Protocol** | Limine (BIOS + UEFI) | ✅ Complete |
| **Entry Point** | `arch/x86_64/start.asm` | ✅ Complete |
| **GDT + TSS** | `arch/x86_64/gdt.asm` | ✅ Complete |
| **IDT** | `arch/x86_64/idt.asm` | ✅ Complete (32 exceptions, 16 IRQs, APIC timer @240, syscall @128) |
| **Paging (Early)** | `arch/x86_64/paging.asm` | ✅ Complete |
| **Context Switch** | `arch/x86_64/context.asm` | ✅ Complete |
| **Exception Handlers** | `arch/x86_64/isr.c` | ✅ Complete with register dump |
| **IRQ Handlers (APIC)** | `arch/x86_64/irq.c` | ✅ Complete (256-vector array, PIC disabled) |
| **APIC/IOAPIC Driver** | `arch/x86_64/apic.c` | ✅ Complete with PIT-calibrated timer (100Hz) |
| **PMM (Bitmap)** | `mm/pmm.c` | ✅ Complete |
| **VMM (4-level)** | `mm/vmm.c` | ✅ Complete with HHDM |
| **Heap (Buddy)** | `mm/heap.c` | ✅ Complete |
| **Scheduler** | `sched/task.c` | ✅ Round-robin with context switch |
| **Syscalls** | `sched/syscalls.c` | ✅ VFS-based fd_table, open/read/write/close/execve |
| **Serial/UART** | `drivers/serial.c` | ✅ 16550 with printf |
| **Framebuffer** | `drivers/vga.c` | ✅ Limine framebuffer console |
| **PS/2 Keyboard** | `drivers/ps2.c` | ✅ Scancode set 1 |
| **PCI Enumeration** | `drivers/pci.c` | ✅ CF8/CFC |
| **VFS Layer** | `fs/vfs.c` | ✅ Path traversal, mount management |
| **TARFS (Initrd)** | `fs/tarfs.c` | ✅ ustar format |
| **RAMFS** | `fs/ramfs.c` | ✅ In-memory filesystem |
| **UNK Loader** | `fs/unk_loader.c` | ✅ Validation, relocation, capabilities |
| **Kernel Main** | `src/kernel.c` | ✅ Full init sequence with initrd loading |

### Userspace Format & Tools

| Component | File | Status |
|-----------|------|--------|
| **.unk Format Spec** | `specs/unk_format.md` | ✅ Complete (128-byte header, program/section headers, capabilities, metadata, signing) |
| **exe2unk Converter** | `tools/exe2unk/exe2unk.c` | ✅ Complete ELF→.unk with capability embedding |
| **Converter Makefile** | `tools/exe2unk/Makefile` | ✅ Complete |

### Build System

| File | Status |
|------|--------|
| `kernel/Makefile` | ✅ Complete (cross-compile, qemu targets) |
| `kernel/linker.ld` | ✅ Higher-half (0xFFFF800000000000) |
| `build_independent.sh` | ✅ Complete ISO builder with Limine |
| `limine.cfg` | ✅ Bootloader config |

---

## ✅ COMPLETED: Linux-based Daily Driver (KDE Plasma)

| Component | Status |
|-----------|--------|
| ArchISO profile | ✅ Complete (`iso-profile/`) |
| KDE Plasma 6 packages | ✅ Configured |
| WhiteSur GTK/KDE theme | ✅ Complete |
| Latte Dock (macOS-style) | ✅ Configured |
| SDDM theme (branded) | ✅ Complete |
| GRUB theme (branded) | ✅ Complete |
| Calamares installer (branded) | ✅ Complete |
| India localization (locale, tz, fonts) | ✅ Complete |
| Default wallpaper (SVG) | ✅ Complete |
| Desktop shortcuts | ✅ Complete |
| Brave browser | ✅ Included |
| GitHub Actions workflow | ✅ `.github/workflows/build-linux-based.yml` |

---

## 🎯 NEXT STEPS (Priority Order)

### 1. Complete Syscall Implementations (Kernel)
- [ ] `fork()` with copy-on-write page tables
- [ ] `execve()` with proper user mode transition (stack setup, arg passing)
- [ ] `mmap()` / `munmap()` / `mprotect()`
- [ ] Signal handling (sigaction, sigreturn)
- [ ] `waitpid()`, `getpid()`, `getuid()`

### 2. Userspace libc
- [ ] Port musl libc to Unknown OS
- [ ] Implement syscall wrappers
- [ ] Standard library (stdio, stdlib, string, etc.)
- [ ] Dynamic linker (ld.unk.so)

### 3. Init System & Core Userspace
- [ ] Proper init (PID 1) with service management
- [ ] Shell with job control, pipes, redirection
- [ ] Core utilities: ls, cat, cp, mv, rm, mkdir, ps, kill
- [ ] Text editor (nano/vim-like)

### 4. Device Drivers
- [ ] AHCI/SATA driver
- [ ] NVMe driver
- [ ] VirtIO (virtio-blk, virtio-net, virtio-gpu)
- [ ] Basic GPU framebuffer driver
- [ ] Network stack (virtio-net → TCP/IP)

### 5. Graphics & Desktop
- [ ] Window manager / compositor
- [ ] Desktop environment (dock, panel, app launcher)
- [ ] Window decorations, workspace management

### 6. Installer
- [ ] Partitioning tool (GPT/MBR)
- [ ] Dual-boot detection (Windows/Linux)
- [ ] Calamares-style graphical installer
- [ ] UEFI + BIOS bootloader installation

---

## 📦 BUILD & TEST STATUS

### Independent Kernel
```bash
# Requires Linux with cross-compiler
./build_independent.sh
# Produces: unknownos.iso (BIOS + UEFI bootable)
```

### Test Commands
```bash
# BIOS
qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk

# UEFI
qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk \
  -bios /usr/share/ovmf/OVMF.fd

# KVM (fast)
qemu-system-x86_64 -cdrom unknownos.iso -serial stdio -display gtk \
  -enable-kvm -cpu host -m 2G
```

### GitHub Actions
- ✅ `.github/workflows/build-independent-kernel.yml` - Builds custom kernel ISO
- ✅ `.github/workflows/build-linux-based.yml` - Builds KDE Plasma ISO

---

## 📁 FILE INVENTORY (Key Files)

### Kernel Source (~45 files)
```
kernel/
├── include/kernel.h                    # 534 lines - Complete API
├── src/kernel.c                        # 316 lines - Entry + init
├── arch/x86_64/
│   ├── start.asm                       # 80 lines - _start
│   ├── gdt.asm                         # ~100 lines
│   ├── idt.asm                         # 320 lines - Full IDT
│   ├── paging.asm                      # ~150 lines
│   ├── context.asm                     # ~80 lines
│   ├── isr.c                           # ~200 lines
│   ├── irq.c                           # 167 lines - APIC IRQ
│   └── apic.c                          # 271 lines - APIC/IOAPIC + timer
├── mm/
│   ├── pmm.c                           # ~200 lines
│   ├── vmm.c                           # ~300 lines
│   └── heap.c                          # ~250 lines
├── sched/
│   ├── task.c                          # 195 lines - Scheduler
│   └── syscalls.c                      # 292 lines - Syscalls + fd_table
├── drivers/
│   ├── serial.c                        # ~150 lines
│   ├── vga.c                           # ~150 lines
│   ├── ps2.c                           # ~200 lines
│   └── pci.c                           # ~200 lines
├── fs/
│   ├── vfs.c                           # 404 lines - VFS
│   ├── tarfs.c                         # ~200 lines
│   ├── ramfs.c                         # ~200 lines
│   └── unk_loader.c                    # ~400 lines - .unk loader
├── linker.ld                           # 55 lines
└── Makefile                            # 94 lines
```

### Userspace Tools
```
tools/exe2unk/
├── exe2unk.c                           # 692 lines - Full ELF→.unk
└── Makefile                            # 20 lines
```

### Build & Config
```
build_independent.sh                    # 227 lines - ISO builder
limine.cfg                              # Bootloader config
.github/workflows/
├── build-independent-kernel.yml        # CI for custom kernel
└── build-linux-based.yml               # CI for KDE Plasma
```

### Linux-based ISO Profile
```
iso-profile/
├── profiledef.sh
├── packages.x86_64
├── airootfs/etc/os-release
├── airootfs/etc/locale.gen
├── airootfs/etc/skel/ (KDE config, shortcuts)
├── airootfs/usr/share/ (wallpaper, themes, SDDM, GRUB, Calamares)
└── ... (complete ArchISO structure)
```

---

## 🏷️ BRANDING (Applied Everywhere)

| Component | Branding |
|-----------|----------|
| OS Name | **Unknown OS** |
| Version | **1.0.0-IND** |
| Tagline | **India's First Independent Operating System** |
| Author | **byrex** |
| Support | **byrex.me@proton.me** |
| Kernel Banner | ASCII art + version + tagline |
| GRUB Theme | Branded background + menu |
| SDDM Theme | Branded login screen |
| Calamares | Branded installer |
| Desktop | Shortcuts: Install, Welcome, Terminal, Browser, Settings |
| Wallpaper | Custom SVG with branding |

---

## 📋 NOTES

- **No Linux code in kernel** - Fully independent, written from scratch
- **Limine bootloader** - Modern, supports BIOS + UEFI, no GRUB dependency
- **Higher-half kernel** - 0xFFFF800000000000, 4-level paging
- **.unk format** - Native executable with capability-based security
- **exe2unk** - Converts ELF → .unk with capability embedding
- **VFS** - Supports multiple filesystems, mount management, path traversal
- **APIC/IOAPIC** - Modern interrupt handling, PIT-calibrated timer
- **Per-task fd_table** - Integrated with VFS, created on first syscall
- **Ready to build** - Run `./build_independent.sh` on Linux with cross-compiler

---

**Next milestone**: Complete `fork()` + `execve()` + userspace libc → Self-hosting Unknown OS