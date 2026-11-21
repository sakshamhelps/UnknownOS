#!/bin/bash
# Unknown OS ISO Builder
# Run on Arch Linux or Arch-based distro (or in Arch Docker container)
# Usage: sudo ./build.sh

set -euo pipefail

# Configuration
PROFILE_DIR="$(dirname "$(realpath "$0")")"
WORK_DIR="/tmp/unknownos-build"
OUT_DIR="${PROFILE_DIR}/out"
ISO_NAME="UnknownOS-1.0.0-IND-x86_64.iso"
ISO_LABEL="UNKNOWN_OS_IND"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${GREEN}[BUILD]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }
info() { echo -e "${BLUE}[INFO]${NC} $*"; }

# Check root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        error "This script must be run as root (sudo ./build.sh)"
    fi
}

# Check dependencies
check_deps() {
    local deps=(archiso mkarchiso pacman git rsync)
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &>/dev/null; then
            error "Missing dependency: $dep (install with: pacman -S $dep)"
        fi
    done
}

# Prepare build environment
prepare_build() {
    log "Preparing build environment..."
    rm -rf "$WORK_DIR"
    mkdir -p "$WORK_DIR"
    mkdir -p "$OUT_DIR"

    # Copy profile to work directory
    rsync -a "$PROFILE_DIR/" "$WORK_DIR/profile/" --exclude="out" --exclude="*.iso" --exclude="build.sh"
}

# Install AUR packages (WhiteSur theme, Brave) in chroot
install_aur_packages() {
    log "Setting up AUR helper and custom packages..."

    # Create a script to run inside the chroot
    cat > "$WORK_DIR/install-aur.sh" << 'CHROOT_EOF'
#!/bin/bash
set -euo pipefail

# Install yay (AUR helper)
cd /tmp
git clone https://aur.archlinux.org/yay.git
cd yay
makepkg -si --noconfirm --needed

# Install WhiteSur themes
yay -S --noconfirm --needed whitesur-gtk-theme-git whitesur-icon-theme-git whitesur-cursor-theme-git

# Install Brave via flatpak (more reliable in build)
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install -y --system flathub com.brave.Browser

# Create Brave desktop entry override for branding
cat > /usr/share/applications/brave-browser.desktop << 'DESKTOP_EOF'
[Desktop Entry]
Version=1.0
Name=Brave Browser
GenericName=Web Browser
Comment=Fast, secure, and private web browser
Exec=/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=brave com.brave.Browser %U
Terminal=false
Icon=com.brave.Browser
Type=Application
Categories=Network;WebBrowser;
MimeType=text/html;text/xml;application/xhtml+xml;application/xml;application/rss+xml;application/rdf+xml;image/gif;image/jpeg;image/png;x-scheme-handler/http;x-scheme-handler/https;x-scheme-handler/ftp;x-scheme-handler/chrome;video/webm;application/x-xpinstall;
StartupNotify=true
StartupWMClass=brave-browser
DESKTOP_EOF

# Set default browser
xdg-settings set default-web-browser brave-browser.desktop 2>/dev/null || true

# Apply WhiteSur theme globally
mkdir -p /etc/gtk-3.0
cat > /etc/gtk-3.0/settings.ini << 'GTK_EOF'
[Settings]
gtk-theme-name=WhiteSur-dark
gtk-icon-theme-name=WhiteSur-dark
gtk-cursor-theme-name=WhiteSur
gtk-font-name=Noto Sans 10
gtk-cursor-theme-size=24
gtk-toolbar-style=GTK_TOOLBAR_BOTH
gtk-toolbar-icon-size=GTK_ICON_SIZE_LARGE_TOOLBAR
gtk-button-images=1
gtk-menu-images=1
gtk-enable-event-sounds=1
gtk-enable-input-feedback-sounds=1
gtk-xft-antialias=1
gtk-xft-hinting=1
gtk-xft-hintstyle=hintslight
gtk-xft-rgba=rgb
GTK_EOF

# Qt theme
mkdir -p /etc/xdg
cat > /etc/xdg/qt5ct.conf << 'QT_EOF'
[Appearance]
standard_dialogs=gtk2
style=gtk2
icon_theme=WhiteSur-dark
font="Noto Sans,10,-1,5,50,0,0,0,0,0"
QT_EOF

cat > /etc/xdg/qt6ct.conf << 'QT_EOF'
[Appearance]
standard_dialogs=gtk2
style=gtk2
icon_theme=WhiteSur-dark
font="Noto Sans,10,-1,5,50,0,0,0,0,0"
QT_EOF

# Environment for Qt
cat > /etc/profile.d/unknownos-qt.sh << 'ENV_EOF'
export QT_QPA_PLATFORMTHEME=gtk2
export QT_STYLE_OVERRIDE=gtk2
export GTK_THEME=WhiteSur-dark
export ICON_THEME=WhiteSur-dark
export CURSOR_THEME=WhiteSur
ENV_EOF

chmod +x /etc/profile.d/unknownos-qt.sh

# Generate locales
locale-gen

# Enable services
systemctl enable NetworkManager sddm bluetooth firewalld cups tlp fstrim.timer systemd-timesyncd systemd-resolved systemd-oomd

# Create unknownos user for live session
useradd -m -G wheel,networkmanager,video,audio,storage,power,lp,scanner,kvm,libvirt -s /bin/zsh liveuser 2>/dev/null || true
echo "liveuser:liveuser" | chpasswd
echo "root:unknownos" | chpasswd

# Configure sudo for liveuser
echo "%wheel ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/99-wheel-nopasswd

# Set hostname
echo "unknownos" > /etc/hostname

# Configure hosts
cat > /etc/hosts << 'HOSTS_EOF'
127.0.0.1   localhost
::1         localhost
127.0.1.1   unknownos.localdomain unknownos
HOSTS_EOF

# Set timezone to India
ln -sf /usr/share/zoneinfo/Asia/Kolkata /etc/localtime
hwclock --systohc

# Regenerate initramfs
mkinitcpio -P

# Update grub
grub-mkconfig -o /boot/grub/grub.cfg

# Clean up
pacman -Sc --noconfirm
rm -rf /tmp/* /var/tmp/*
journalctl --vacuum-time=1d
CHROOT_EOF

    chmod +x "$WORK_DIR/install-aur.sh"
}

# Build the ISO using archiso
build_iso() {
    log "Building ISO with archiso..."

    # Create profile directory structure for archiso
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/.config"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Desktop"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Documents"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Downloads"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Pictures"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Videos"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Music"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Public"
    mkdir -p "$WORK_DIR/profile/airootfs/etc/skel/Templates"

    # Copy our airootfs overlay
    rsync -a "$PROFILE_DIR/airootfs/" "$WORK_DIR/profile/airootfs/" --exclude="*.log"

    # Copy boot configs
    mkdir -p "$WORK_DIR/profile/syslinux"
    cp "$PROFILE_DIR/syslinux/syslinux.cfg" "$WORK_DIR/profile/syslinux/"

    mkdir -p "$WORK_DIR/profile/efiboot/loader/entries"
    cp "$PROFILE_DIR/efiboot/loader/entries/"*.conf "$WORK_DIR/profile/efiboot/loader/entries/"

    # Copy profiledef.sh
    cp "$PROFILE_DIR/profiledef.sh" "$WORK_DIR/profile/"

    # Copy packages list
    cp "$PROFILE_DIR/packages.x86_64" "$WORK_DIR/profile/packages.x86_64"

    # Run archiso build
    cd "$WORK_DIR"
    mkarchiso -v -w "$WORK_DIR/work" -o "$OUT_DIR" "$WORK_DIR/profile/"
}

# Main
main() {
    log "Starting Unknown OS ISO Build..."
    log "Profile: $PROFILE_DIR"
    log "Output: $OUT_DIR/$ISO_NAME"

    check_root
    check_deps
    prepare_build
    install_aur_packages
    build_iso

    log "Build complete!"
    log "ISO location: $OUT_DIR/$ISO_NAME"
    log ""
    log "To test in QEMU:"
    log "  qemu-system-x86_64 -enable-kvm -m 4G -cdrom $OUT_DIR/$ISO_NAME"
    log ""
    log "To write to USB:"
    log "  sudo dd if=$OUT_DIR/$ISO_NAME of=/dev/sdX bs=4M status=progress && sync"
}

main "$@"