# Unknown OS Development Plan - "Everyday Use" Roadmap

To make **Unknown OS** a "complete" everyday-use operating system, we need to transition from core kernel development to a full-featured desktop environment.

## Phase 1: Core System Stability (Kernel & Driver Foundation)
- [ ] **Memory Management:** Finalize PMM, VMM (Paging), and Heap.
- [ ] **Interrupts/Exceptions:** Complete GDT/IDT/ISR handling.
- [ ] **Drivers:** Essential hardware support:
    - [ ] **Keyboard & Mouse (PS/2 + USB HID)**
    - [ ] **Display (GOP / VESA LFB)**
    - [ ] **Storage (NVMe / SATA / AHCI)**
    - [ ] **Network (Intel e1000 / Realtek / WiFi)**
- [ ] **Scheduler:** Preemptive multitasking and process synchronization.

## Phase 2: Desktop Environment (The macOS-like UX)
- [ ] **Window Server:** Native graphics compositor with hardware acceleration.
- [ ] **UI Kit (IndAppKit):** Recreate the "Aqua" look (translucency, blur, drop shadows).
- [ ] **The Dock & Menu Bar:**
    - [ ] Dock: Magnification, application launching, state indicator.
    - [ ] Menu Bar: Global menus, system tray icons, system clock, user session.
- [ ] **Finder:** Native file manager with support for custom filesystem (UnknownFS).
- [ ] **System Preferences:** Modular configuration app for all system/user settings.

## Phase 3: Application Suite
- [ ] **Browser (Integrated):** Porting a headless/minimalist browser engine or deep-integrating **Brave** (Requires Linux/Unix compatibility layer or native port).
- [ ] **Native Apps:**
    - [ ] Terminal (zsh integration)
    - [ ] Mail (PGP-enabled)
    - [ ] Notes (Cloud-sync enabled)
    - [ ] Activity Monitor (System usage)
- [ ] **India-Specific Features:**
    - [ ] UPI (Unified Payments Interface) Secure Plugin
    - [ ] Aadhaar Authentication Module
    - [ ] Regional Multi-Language Input Methods

## Phase 4: Installation & Deployment
- [ ] **Installer:** Graphical user-friendly installation (Dual-boot logic: resizing Windows/Linux partitions).
- [ ] **Recovery:** Built-in partition for system resets and repairs.
- [ ] **ISO Builder:** Final packaging into bootable ISO and live-bootable USB images.

---

### Implementation Strategy: The "Brave" Browser Challenge
Since **Brave** is Chromium-based, it expects a mature POSIX environment (Linux/Windows/macOS). For Unknown OS to support it, we have two options:
1. **Compatibility Layer:** Implement a POSIX-compliant system call layer in the kernel.
2. **Native Port:** Port Chromium's build system to target the Unknown OS ABI (Advanced, very high effort).

*Recommendation:* Start by building a minimal WebKit-based internal browser for simple tasks, then focus on porting a stable Chromium build as an "add-on" package later.

---

### Immediate Action Item: Setting up the Build System
Create the `build.sh` script to manage this complexity.

```bash
#!/bin/bash
# Unknown OS Build System
set -e

echo "Starting build for Unknown OS..."

# Build Kernel
make -C kernel/ all

# Build Userspace
make -C userspace/ all

# Build ISO
./tools/mkiso.py --output=unknown_os_release.iso --kernel=kernel/kernel.bin --initrd=userspace/initrd.tar

echo "Build complete: unknown_os_release.iso created."
```
(I have created this script structure for you to populate with your specific build rules).
