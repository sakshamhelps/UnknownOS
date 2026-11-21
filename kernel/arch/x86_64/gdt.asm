; Unknown OS - GDT Setup
; Author: byrex <byrex.me@proton.me>

bits 64
section .text
global gdt_init
extern gdt64
extern tss

gdt_init:
    ; Load GDT
    lgdt [gdt64.ptr]

    ; Reload segment registers
    mov ax, 0x10        ; Data segment (kernel data)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Reload CS with far jump
    push 0x08           ; Code segment (kernel code)
    lea rax, [rel .flush_cs]
    push rax
    retfq

.flush_cs:
    ; Load TSS
    mov ax, 0x28        ; TSS segment
    ltr ax
    ret

section .rodata
; 64-bit GDT
align 8
gdt64:
.null:      dq 0x0000000000000000           ; 0x00 - Null descriptor
.code:      dq 0x00AF9A000000FFFF           ; 0x08 - Kernel code: 64-bit, present, DPL=0, execute/read
.data:      dq 0x00AF92000000FFFF           ; 0x10 - Kernel data: present, DPL=0, read/write
.user_data: dq 0x00AFF2000000FFFF           ; 0x18 - User data: present, DPL=3, read/write
.user_code: dq 0x00AFFA000000FFFF           ; 0x20 - User code: 64-bit, present, DPL=3, execute/read
.tss:       dq 0x0000890000000000           ; 0x28 - TSS (low)
            dq 0x0000000000000000           ; 0x30 - TSS (high)
.ptr:
    dw $ - gdt64 - 1
    dq gdt64