# Unknown OS - ISO Build Instructions

## Prerequisites

You **must** build on a Linux machine (Arch Linux recommended) or in an Arch Linux container/VM.  
**Cannot build directly on Windows** — ArchISO tools require Linux kernel.

### Option 1: Build in Arch Linux VM (Recommended)
1. Install Arch Linux in VirtualBox/VMware/QEMU
2. Run the build inside the VM

### Option 2: Build in Docker (Any Linux Host)
```bash
# Pull Arch Linux image
docker run -it --privileged -v $(pwd):/build archlinux:latest

# Inside container:
pacman -Syu --noconfirm base-devel archiso git sudo
cd /build/UnknownOS/iso-profile
sudo ./build.sh
```

### Option 3: Build in WSL2 (Windows)
```bash
# Install Arch in WSL2
# https://github.com/yuk7/ArchWSL
# Then run build inside WSL2 Arch
```

---

## Build Steps

### 1. Prepare Build Machine (Arch Linux)
```bash
# Update system
sudo pacman -Syu

# Install build dependencies
sudo pacman -S --needed archiso git rsync make

# Clone this repo (or copy the iso-profile folder)
git clone <your-repo> UnknownOS
cd UnknownOS/iso-profile
```

### 2. Run Build
```bash
# Make script executable
chmod +x build.sh

# Run as root (required for archiso)
sudo ./build.sh
```

### 3. Wait for Build
- **First build:** 20-45 minutes (downloads all packages)
- **Subsequent builds:** 5-15 minutes (cached packages)
- Output: `out/UnknownOS-1.0.0-IND-x86_64.iso`

---

## Test the ISO

### QEMU (Best for testing)
```bash
# BIOS mode
qemu-system-x86_64 -enable-kvm -m 4G -cdrom out/UnknownOS-1.0.0-IND-x86_64.iso

# UEFI mode (requires OVMF)
qemu-system-x86_64 -enable-kvm -m 4G \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2-ovmf/x64/OVMF_CODE.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS.fd \
  -cdrom out/UnknownOS-1.0.0-IND-x86_64.iso
```

### VirtualBox/VMware
- Create new VM → Linux / Arch Linux (64-bit)
- Mount ISO as optical drive
- Enable EFI for UEFI testing

### Write to USB (Real Hardware)
```bash
# Find your USB device (CAREFUL - this DESTROYS data)
lsblk

# Write ISO
sudo dd if=out/UnknownOS-1.0.0-IND-x86_64.iso of=/dev/sdX bs=4M status=progress && sync
```

---

## What's Included in the ISO

### Desktop Environment
- **KDE Plasma 6** with **Latte Dock** (macOS-style bottom dock)
- **WhiteSur Theme** (macOS Big Sur/Monterey clone)
- **WhiteSur Icons & Cursors**
- **SDDM Login** with Unknown OS branding

### Applications
- **Brave Browser** (via Flatpak) - default browser
- **Konsole** - Terminal
- **Dolphin** - File Manager
- **Kate** - Text Editor
- **Discover** - Software Center
- **System Settings** - Control Center
- **Calamares** - Graphical Installer

### India-Specific Features
- **Locale:** `en_IN.UTF-8` (English India)
- **Timezone:** Asia/Kolkata (IST)
- **Keyboard:** US with IBus for Indian languages
- **Input Methods:** InScript, Phonetic for 22 Indian languages
- **Fonts:** Noto fonts covering all Indian scripts

### Boot Support
- **BIOS (Legacy):** Syslinux bootloader
- **UEFI:** systemd-boot
- **Secure Boot:** Compatible (needs MOK enrollment for NVIDIA)

### Installer (Calamares)
- Graphical partitioning (auto/manual)
- Dual-boot detection (os-prober)
- User creation with sudo
- Full disk encryption option
- LVM support

---

## Customization

### Add/Remove Packages
Edit `packages.x86_64` — one package per line.

### Change Theme
Replace WhiteSur references in:
- `airootfs/etc/skel/.config/kdeglobals`
- `airootfs/etc/gtk-3.0/settings.ini`
- `airootfs/etc/profile.d/unknownos-qt.sh`

### Add Custom Apps
Place `.desktop` files in `airootfs/usr/share/applications/`

### Custom Wallpapers
Add images to `airootfs/usr/share/wallpapers/`

### Branding
Edit `airootfs/etc/os-release` for OS name/version.

---

## Troubleshooting

### Build Fails with "mkarchiso not found"
```bash
sudo pacman -S archiso
```

### Out of Disk Space
- Build needs ~15GB free
- Clean: `sudo rm -rf /tmp/unknownos-build`

### Brave Not Installing
- Requires internet during build
- Flatpak installation happens in chroot

### UEFI Boot Fails
- Ensure VM has EFI enabled
- For real hardware: disable Secure Boot or enroll MOK

### Dual Boot Not Detected
- Run `os-prober` after install
- `grub-mkconfig -o /boot/grub/grub.cfg`

---

## Next Steps After Testing

1. **Test thoroughly** in VM (live boot + install)
2. **Test on real hardware** (USB boot)
3. **Verify:** WiFi, audio, Bluetooth, sleep/wake, printers
4. **Customize** wallpapers, default apps, settings
5. **Distribute** ISO to users

---

## Support

- **Email:** byrex.me@proton.me
- **Issues:** Report build failures with full log
- **Documentation:** https://byrex.me/docs

---

## License

Proprietary — Unknown OS by byrex.me@proton.me  
Based on Arch Linux (GPL/Various) — upstream licenses apply.