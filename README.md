# Unknown OS - India's Own Operating System

**Developed by:** byrex.me@proton.me  
**Support:** byrex.me@proton.me  
**Target:** India-first operating system with macOS-like experience  
**Architecture:** x86_64, ARM64 (future)  
**Kernel:** Custom microkernel (not Linux-based)

## Overview

Unknown OS is a fully independent, from-scratch operating system designed specifically for India. It features a macOS-inspired user experience with a custom kernel, native window server, dock, and all system components built from the ground up.

## Features

### Core System
- **Custom Microkernel** - Preemptive multitasking, memory protection, IPC, capability-based security
- **Bootloader** - Dual BIOS (legacy) and UEFI support with secure boot
- **Virtual Memory** - Paging, demand paging, copy-on-write
- **Process Management** - Threads, processes, scheduling (CFS-like)
- **Filesystem** - Custom FS (UnknownFS) with journaling, encryption, snapshots
- **Device Drivers** - Native driver framework (PCIe, USB, NVMe, GPU, Audio, Network)

### User Experience (macOS-like)
- **Aqua-inspired UI** - Translucency, blur effects, smooth animations
- **Dock** - Magnification, running indicators, stacks, recent apps
- **Mission Control** - Spaces, Exposé, window overview
- **Spotlight Search** - Instant file/app/system search
- **Notification Center** - Banners, alerts, widgets
- **Control Center** - Quick toggles (WiFi, Bluetooth, AirDrop, Display, Sound)
- **Launchpad** - Full-screen app launcher with folders
- **System Preferences** - Comprehensive settings panel

### India-Specific Features
- **Multi-language Support** - 22 official languages + regional scripts
- **Indian Keyboard Layouts** - InScript, phonetic, transliteration
- **Aadhaar Integration** - Secure authentication framework
- **UPI Payment Integration** - System-level payment APIs
- **DigiLocker Integration** - Document storage
- **Regional Date/Time/Calendar** - Hindu, Islamic, regional calendars
- **India Timezone (IST)** - Default with DST awareness
- **Local Compliance** - Data localization, IT Act 2000 compliance

### Installation & Deployment
- **Live USB Creator** - Bootable USB with persistence
- **Dual Boot Manager** - Windows/Linux/Unknown OS coexistence
- **Virtual Boot** - VMware, VirtualBox, Hyper-V, QEMU images
- **UEFI Secure Boot** - Custom keys, MOK management
- **BIOS Legacy Boot** - MBR/GPT hybrid support
- **Network Install** - PXE boot, netinstall
- **Recovery Partition** - System recovery, diagnostics, reinstall

### Default Applications
- **Finder** - File manager with tabs, preview, tags
- **Safari-like Browser** - WebKit-based, privacy-focused
- **Mail** - Email client with PGP support
- **Messages** - iMessage-compatible, RCS, SMS
- **FaceTime-like** - Video/audio calls, screen sharing
- **Notes** - Rich text, checklists, attachments
- **Reminders** - Location-based, smart lists
- **Calendar** - Multiple calendars, CalDAV
- **Photos** - ML-powered organization, editing
- **Music** - Streaming, local library, lossless
- **TV** - Video playback, streaming
- **App Store** - Sandboxed apps, reviews, updates
- **Terminal** - zsh/fish, GPU-accelerated
- **Activity Monitor** - Process, energy, network, disk
- **Disk Utility** - Partition, format, repair, encrypt
- **Keychain Access** - Passwords, certificates, keys

## Project Structure

```
UnknownOS/
├── bootloader/           # BIOS & UEFI bootloaders
│   ├── bios/             # Legacy BIOS bootloader (assembly)
│   ├── uefi/             # UEFI bootloader (C)
│   └── common/           # Shared boot protocols
├── kernel/               # Custom microkernel
│   ├── arch/             # Architecture-specific code
│   │   ├── x86_64/       # x86_64 implementation
│   │   └── arm64/        # ARM64 (future)
│   ├── mm/               # Memory management
│   ├── sched/            # Scheduler
│   ├── ipc/              # Inter-process communication
│   ├── vfs/              # Virtual filesystem
│   ├── fs/               # Filesystem drivers
│   ├── drivers/          # Device drivers
│   ├── security/         # Capabilities, MAC, encryption
│   └── syscalls/         # System call interface
├── userspace/            # User-space components
│   ├── init/             # Init system (launchd-like)
│   ├── libc/             # Custom C library
│   ├── libui/            # UI toolkit (CoreGraphics-like)
│   ├── windowserver/     # Window compositor
│   ├── dock/             # Dock application
│   ├── finder/           # File manager
│   ├── spotlight/        # Search indexer
│   ├── controlcenter/    # Control Center
│   ├── notificationcenter/ # Notifications
│   ├── launchpad/        # App launcher
│   ├── missioncontrol/   # Window management
│   ├── systempreferences/# Settings app
│   ├── terminal/         # Terminal emulator
│   ├── activitymonitor/  # System monitor
│   ├── diskutility/      # Disk management
│   ├── keychain/         # Credential manager
│   ├── safari/           # Web browser
│   ├── mail/             # Email client
│   ├── messages/         # Messaging
│   ├── facetime/         # Video calls
│   ├── notes/            # Notes app
│   ├── reminders/        # Reminders
│   ├── calendar/         # Calendar
│   ├── photos/           # Photo management
│   ├── music/            # Music player
│   ├── tv/               # Video player
│   └── appstore/         # App store client
├── installer/            # Installation system
│   ├── liveusb/          # Live USB creator
│   ├── dualboot/         # Dual boot manager
│   ├── virtualboot/      # VM image builders
│   ├── netinstall/       # Network installer
│   └── recovery/         # Recovery partition
├── india/                # India-specific modules
│   ├── languages/        # Language packs
│   ├── keyboards/        # Keyboard layouts
│   ├── aadhaar/          # Aadhaar integration
│   ├── upi/              # UPI payments
│   ├── digilocker/       # DigiLocker
│   ├── calendar/         # Regional calendars
│   └── compliance/       # Legal compliance
├── wallpapers/           # Default wallpapers
├── themes/               # System themes
├── fonts/                # System fonts (including Indian scripts)
├── docs/                 # Documentation
├── tools/                # Build tools
├── scripts/              # Build/deploy scripts
└── tests/                # Test suites
```

## Building

### Prerequisites
- Cross-compiler toolchain (x86_64-unknownos-)
- NASM/YASM for assembly
- QEMU for testing
- Python 3.10+ for build scripts
- CMake 3.20+
- Ninja build system

### Quick Build
```bash
./scripts/build.sh all
```

### Build Targets
- `bootloader` - Build BIOS and UEFI bootloaders
- `kernel` - Build kernel
- `userspace` - Build all user-space apps
- `installer` - Build installation media
- `iso` - Create bootable ISO
- `vm` - Create VM images (VMDK, VDI, QCOW2)
- `liveusb` - Create live USB image
- `all` - Build everything

## Testing

```bash
# Run in QEMU (BIOS)
./scripts/run_qemu.sh bios

# Run in QEMU (UEFI)
./scripts/run_qemu.sh uefi

# Run kernel tests
./scripts/test_kernel.sh

# Run userspace tests
./scripts/test_userspace.sh
```

## Installation

### Dual Boot (Windows + Unknown OS)
1. Create live USB: `./scripts/create_liveusb.sh /dev/sdX`
2. Boot from USB
3. Run installer: Select "Install alongside Windows"
4. Follow prompts

### Virtual Machine
1. Build VM image: `./scripts/build.sh vm`
2. Import VMDK/VDI/QCOW2 into your hypervisor
3. Boot and enjoy

### UEFI Secure Boot
1. Enroll custom keys via MOK Manager
2. Or disable Secure Boot in BIOS

## Support

**Email:** byrex.me@proton.me  
**Issues:** GitHub Issues  
**Documentation:** docs/ directory  

## License

Proprietary - Unknown OS is developed by byrex.me@proton.me  
All rights reserved. See LICENSE file for details.

## Contributing

This is a private project. For collaboration inquiries, contact byrex.me@proton.me

---

**Unknown OS** — *India's own operating system, built from scratch, for India.*