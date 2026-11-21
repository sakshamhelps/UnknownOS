; Unknown OS - x86_64 Kernel Entry Point
; Booted by Limine (BIOS/UEFI) → starts in long mode, higher-half mapped
; Author: byrex <byrex.me@proton.me>
; India's First Independent Operating System

bits 64
section .text
global _start
extern kernel_main
extern gdt_init
extern idt_init
extern pmm_init
extern vmm_init
extern serial_init
extern vga_init
extern ps2_init
extern pci_init
extern sched_init
extern syscall_init

; Limine boot info structure pointer passed in RDI (per Limine protocol)
_start:
    ; ---------------------------------------------------------
    ; 1. Basic CPU setup - already in long mode by Limine
    ; ---------------------------------------------------------
    cli                     ; Disable interrupts until IDT ready

    ; ---------------------------------------------------------
    ; 2. Initialize core subsystems
    ; ---------------------------------------------------------
    call gdt_init           ; Load GDT (code/data/TSS)
    call idt_init           ; Load IDT (ISR handlers)
    call serial_init        ; Early logging via COM1
    call vga_init           ; Framebuffer console
    call pmm_init           ; Physical memory manager
    call vmm_init           ; Virtual memory manager (higher-half)
    call ps2_init           ; Keyboard controller
    call pci_init           ; PCI bus enumeration
    call sched_init         ; Task scheduler
    call syscall_init       ; Syscall entry (MSR_IA32_LSTAR)

    ; ---------------------------------------------------------
    ; 3. Print banner
    ; ---------------------------------------------------------
    mov rdi, banner
    call serial_puts
    call vga_puts

    ; ---------------------------------------------------------
    ; 4. Enter C main (never returns)
    ; ---------------------------------------------------------
    mov rdi, [rel boot_info]    ; Pass Limine boot info to kernel_main
    call kernel_main

    ; ---------------------------------------------------------
    ; 5. Should never reach here
    ; ---------------------------------------------------------
.halt:
    hlt
    jmp .halt

; ---------------------------------------------------------
; Data
; ---------------------------------------------------------
section .rodata
banner: db 10, 10
        db "    ███████╗███████╗██████╗ ██╗   ██╗███████╗██████╗ ", 10
        db "    ██╔════╝██╔════╝██╔══██╗██║   ██║██╔════╝██╔══██╗", 10
        db "    ███████╗███████╗██████╔╝██║   ██║███████╗██████╔╝", 10
        db "    ╚════██║╚════██║██╔═══╝ ██║   ██║╚════██║██╔══██╗", 10
        db "    ███████║███████║██║     ╚██████╔╝███████║██║  ██║", 10
        db "    ╚══════╝╚══════╝╚═╝      ╚═════╝ ╚══════╝╚═╝  ╚═╝", 10
        db 10
        db "    Unknown OS 1.0.0-IND  |  India's First Independent OS", 10
        db "    byrex.me@proton.me  |  Built from scratch, not Linux", 10
        db 10, 0

section .bss
boot_info: resq 1

; ---------------------------------------------------------
; External C functions (forward declarations)
; ---------------------------------------------------------
extern serial_puts
extern vga_puts