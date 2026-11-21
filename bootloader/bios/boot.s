; Unknown OS BIOS Bootloader Entry Point
; Compatible with Multiboot2 Specification

MB2_MAGIC    equ 0xe85250d6
MB2_ARCH     equ 0 ; i386 / x86_64 protected mode entry
MB2_LENGTH   equ multiboot_header_end - multiboot_header
MB2_CHECKSUM equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

section .multiboot_header
align 8
multiboot_header:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LENGTH
    dd MB2_CHECKSUM

    ; Framebuffer tag for high resolution UI (macOS style 1920x1080)
    dw 5 ; type = framebuffer tag
    dw 0 ; flags
    dd 20 ; size
    dd 1920 ; width
    dd 1080 ; height
    dd 32   ; depth (BPP)

    ; End tag
    dw 0
    dw 0
    dd 8
multiboot_header_end:

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top

    ; Pass multiboot magic & info structure to kernel_main
    push ebx
    push eax

    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 65536 ; 64 KiB kernel stack
stack_top:
