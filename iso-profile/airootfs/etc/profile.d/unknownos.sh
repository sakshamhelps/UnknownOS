#!/bin/bash
# Unknown OS Environment Variables

export UNKNOWN_OS_VERSION="1.0.0-IND"
export UNKNOWN_OS_NAME="Unknown OS"
export UNKNOWN_OS_SUPPORT="byrex.me@proton.me"

# Qt/Gtk theming
export QT_QPA_PLATFORMTHEME=gtk2
export QT_STYLE_OVERRIDE=gtk2
export GTK_THEME=WhiteSur-dark
export ICON_THEME=WhiteSur-dark
export CURSOR_THEME=WhiteSur

# Default applications
export BROWSER=brave-browser
export EDITOR=kate
export TERMINAL=konsole
export FILE_MANAGER=dolphin

# India locale
export LANG=en_IN.UTF-8
export LC_ALL=en_IN.UTF-8

# Path
export PATH="$HOME/.local/bin:$PATH"

# Welcome message for interactive shells
if [[ $- == *i* ]] && [[ -t 1 ]] && [[ -z "$UNKNOWN_OS_WELCOME_SHOWN" ]]; then
    export UNKNOWN_OS_WELCOME_SHOWN=1
    fastfetch 2>/dev/null || neofetch 2>/dev/null || echo "Welcome to Unknown OS 1.0.0-IND"
    echo "Support: byrex.me@proton.me"
    echo "Run 'unknownos-help' for commands"
    echo
fi

# Aliases
alias ls='ls --color=auto --group-directories-first'
alias ll='ls -la'
alias la='ls -la'
alias grep='grep --color=auto'
alias df='df -h'
alias du='du -h'
alias free='free -h'
alias ps='ps auxf'
alias ports='ss -tulnp'
alias update='sudo pacman -Syu && flatpak update -y'
alias install='sudo pacman -S'
alias search='pacman -Ss'
alias clean='sudo pacman -Sc && flatpak uninstall --unused -y'
alias unknownos-help='echo "Unknown OS Commands:"; echo "  update      - Update system"; echo "  install     - Install package"; echo "  search      - Search packages"; echo "  clean       - Clean cache"; echo "  unknownos-help - Show this help"'