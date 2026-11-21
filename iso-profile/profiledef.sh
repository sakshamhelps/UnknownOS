#!/bin/bash
# Unknown OS - ArchISO Profile Definition
# Build: sudo ./build.sh -p unknownos -o /out

profile_name="unknownos"
profile_description="Unknown OS - India's Independent Operating System"
profile_arch="x86_64"

# ISO label (max 32 chars)
iso_name="UnknownOS"
iso_label="UNKNOWN_OS_IND"
iso_publisher="byrex <byrex.me@proton.me>"
iso_application="Unknown OS Live/Install"

# Boot modes
boot_modes=('bios.syslinux.mbr' 'bios.syslinux.eltorito' 'uefi-x64.systemd-boot.esp' 'uefi-x64.systemd-boot.eltorito')

# Packages to install
packages=(
    # Base system
    'base' 'linux' 'linux-firmware' 'linux-headers'
    'systemd' 'systemd-sysvcompat'

    # Installer
    'calamares' 'calamares-settings-archlinux'

    # Desktop Environment - KDE Plasma (best for macOS-like customization)
    'plasma' 'plasma-wayland-session' 'konsole' 'dolphin' 'kate' 'kwrite'
    'kde-gtk-config' 'breeze-gtk' 'oxygen' 'qt5ct' 'qt6ct'

    # Latte Dock (macOS-style dock)
    'latte-dock'

    # Display/Login Manager
    'sddm'

    # Network
    'networkmanager' 'network-manager-applet' 'modemmanager'

    # Audio
    'pipewire' 'pipewire-pulse' 'pipewire-alsa' 'pipewire-jack' 'wireplumber'
    'alsa-utils' 'pavucontrol'

    # Bluetooth
    'bluez' 'bluez-utils' 'bluedevil'

    # Fonts (including Indian scripts)
    'noto-fonts' 'noto-fonts-emoji' 'noto-fonts-cjk' 'noto-fonts-extra'
    'ttf-dejavu' 'ttf-liberation' 'ttf-roboto' 'ttf-ubuntu-font-family'
    'adobe-source-han-sans-otc-fonts' 'adobe-source-han-serif-otc-fonts'

    # Browser - Brave (from AUR, we'll handle via custom repo or flatpak)
    'flatpak' 'flatpak-builder'

    # System utilities
    'firewalld' 'ufw' 'gufw'
    'htop' 'btop' 'neofetch' 'fastfetch'
    'vim' 'nano' 'micro'
    'git' 'wget' 'curl' 'rsync' 'unzip' 'p7zip' 'unrar'
    'ntfs-3g' 'exfatprogs' 'dosfstools' 'e2fsprogs' 'btrfs-progs' 'xfsprogs'
    'udisks2' 'gvfs' 'gvfs-mtp' 'gvfs-nfs' 'gvfs-smb'
    'xdg-user-dirs' 'xdg-utils'

    # India-specific
    'ibus' 'ibus-typing-booster' 'ibus-m17n' 'm17n-db' 'm17n-contrib'

    # Graphics/Drivers
    'mesa' 'vulkan-radeon' 'vulkan-intel' 'vulkan-nouveau'
    'nvidia-dkms' 'nvidia-utils' 'nvidia-settings'
    'xf86-video-amdgpu' 'xf86-video-ati' 'xf86-video-intel' 'xf86-video-nouveau'

    # Printing
    'cups' 'cups-pdf' 'system-config-printer'

    # Power management
    'power-profiles-daemon' 'tlp' 'tlp-rdw'

    # ArchISO requirements
    'archiso' 'mkinitcpio' 'mkinitcpio-archiso'
)

# AUR packages (built separately or via flatpak)
aur_packages=(
    'brave-bin'
    'whitesur-gtk-theme-git'
    'whitesur-icon-theme-git'
    'whitesur-cursor-theme-git'
    'mcFly'  # better history
    'starship'  # prompt
)

# Services to enable
services=(
    'NetworkManager'
    'sddm'
    'bluetooth'
    'pipewire'
    'pipewire-pulse'
    'wireplumber'
    'firewalld'
    'cups'
    'tlp'
    'fstrim.timer'
    'systemd-timesyncd'
    'systemd-resolved'
    'systemd-oomd'
)

# File systems modules for initramfs
mkinitcpio_modules=(
    'loop' 'squashfs' 'overlay' 'vfat' 'ext4' 'btrfs' 'xfs' 'ntfs3'
)

# Kernel command line options
kernel_cmdline="quiet splash loglevel=3 rd.udev.log_level=3 vt.global_cursor_default=0"

# Compression
compression="zstd"
compression_options=(-T0 -19)

# Build directory
work_dir="/tmp/archiso-build"
out_dir="/out"

# GPG key for signing (optional)
gpg_key=""

# Profile-specific overrides
profile_packages() {
    return 0
}

profile_packages_bios() {
    return 0
}

profile_packages_uefi() {
    return 0
}

# Custom installation steps
profile_install() {
    # Handled by airootfs overlay
    return 0
}