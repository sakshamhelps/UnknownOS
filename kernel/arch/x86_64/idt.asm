; Unknown OS - Interrupt Descriptor Table
; Author: byrex <byrex.me@proton.me>

bits 64
section .text

extern isr_stub_table
extern irq_stub_table
extern syscall_entry

global idt_load
global idt_ptr

; IDT entry structure (16 bytes each)
; offset_low (16), selector (16), ist (8), type_attr (8), offset_mid (16), offset_high (32), reserved (32)

section .data
align 16
idt_ptr:
    dw 256 * 16 - 1    ; Limit
    dq idt_table       ; Base

align 16
idt_table:
    times 256 dq 0, 0  ; 256 entries * 16 bytes

section .text

; void idt_load(void)
idt_load:
    lidt [idt_ptr]
    ret

; Macro to set IDT entry
%macro SET_IDT_ENTRY 3
    ; %1 = index, %2 = handler address, %3 = type_attr
    mov rax, %2
    mov word [idt_table + %1 * 16 + 0], ax        ; offset_low
    mov word [idt_table + %1 * 16 + 2], 0x08      ; selector (kernel code segment)
    mov byte [idt_table + %1 * 16 + 4], 0         ; IST = 0
    mov byte [idt_table + %1 * 16 + 5], %3        ; type_attr
    shr rax, 16
    mov word [idt_table + %1 * 16 + 6], ax        ; offset_mid
    shr rax, 16
    mov dword [idt_table + %1 * 16 + 8], eax      ; offset_high
    mov dword [idt_table + %1 * 16 + 12], 0       ; reserved
%endmacro

; void idt_init(void)
global idt_init
idt_init:
    ; Exceptions (0-31): present, DPL=0, 64-bit interrupt gate (0x8E)
    SET_IDT_ENTRY 0,  isr_stub_table + 0 * 16,  0x8E   ; #DE Divide Error
    SET_IDT_ENTRY 1,  isr_stub_table + 1 * 16,  0x8E   ; #DB Debug
    SET_IDT_ENTRY 2,  isr_stub_table + 2 * 16,  0x8E   ; NMI
    SET_IDT_ENTRY 3,  isr_stub_table + 3 * 16,  0x8E   ; #BP Breakpoint
    SET_IDT_ENTRY 4,  isr_stub_table + 4 * 16,  0x8E   ; #OF Overflow
    SET_IDT_ENTRY 5,  isr_stub_table + 5 * 16,  0x8E   ; #BR Bound Range
    SET_IDT_ENTRY 6,  isr_stub_table + 6 * 16,  0x8E   ; #UD Invalid Opcode
    SET_IDT_ENTRY 7,  isr_stub_table + 7 * 16,  0x8E   ; #NM Device Not Available
    SET_IDT_ENTRY 8,  isr_stub_table + 8 * 16,  0x8E   ; #DF Double Fault
    SET_IDT_ENTRY 9,  isr_stub_table + 9 * 16,  0x8E   ; Coprocessor Segment Overrun
    SET_IDT_ENTRY 10, isr_stub_table + 10 * 16, 0x8E   ; #TS Invalid TSS
    SET_IDT_ENTRY 11, isr_stub_table + 11 * 16, 0x8E   ; #NP Segment Not Present
    SET_IDT_ENTRY 12, isr_stub_table + 12 * 16, 0x8E   ; #SS Stack Segment Fault
    SET_IDT_ENTRY 13, isr_stub_table + 13 * 16, 0x8E   ; #GP General Protection
    SET_IDT_ENTRY 14, isr_stub_table + 14 * 16, 0x8E   ; #PF Page Fault
    SET_IDT_ENTRY 15, isr_stub_table + 15 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 16, isr_stub_table + 16 * 16, 0x8E   ; #MF x87 FPU
    SET_IDT_ENTRY 17, isr_stub_table + 17 * 16, 0x8E   ; #AC Alignment Check
    SET_IDT_ENTRY 18, isr_stub_table + 18 * 16, 0x8E   ; #MC Machine Check
    SET_IDT_ENTRY 19, isr_stub_table + 19 * 16, 0x8E   ; #XM SIMD FPU
    SET_IDT_ENTRY 20, isr_stub_table + 20 * 16, 0x8E   ; #VE Virtualization
    SET_IDT_ENTRY 21, isr_stub_table + 21 * 16, 0x8E   ; #CP Control Protection
    SET_IDT_ENTRY 22, isr_stub_table + 22 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 23, isr_stub_table + 23 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 24, isr_stub_table + 24 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 25, isr_stub_table + 25 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 26, isr_stub_table + 26 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 27, isr_stub_table + 27 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 28, isr_stub_table + 28 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 29, isr_stub_table + 29 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 30, isr_stub_table + 30 * 16, 0x8E   ; Reserved
    SET_IDT_ENTRY 31, isr_stub_table + 31 * 16, 0x8E   ; Reserved

    ; IRQs via IOAPIC (32-47): present, DPL=0, 64-bit interrupt gate (0x8E)
    SET_IDT_ENTRY 32, irq_stub_table + 0 * 16,  0x8E   ; IRQ0 - Timer (legacy, masked)
    SET_IDT_ENTRY 33, irq_stub_table + 1 * 16,  0x8E   ; IRQ1 - Keyboard
    SET_IDT_ENTRY 34, irq_stub_table + 2 * 16,  0x8E   ; IRQ2 - Cascade
    SET_IDT_ENTRY 35, irq_stub_table + 3 * 16,  0x8E   ; IRQ3 - COM2
    SET_IDT_ENTRY 36, irq_stub_table + 4 * 16,  0x8E   ; IRQ4 - COM1
    SET_IDT_ENTRY 37, irq_stub_table + 5 * 16,  0x8E   ; IRQ5 - LPT2
    SET_IDT_ENTRY 38, irq_stub_table + 6 * 16,  0x8E   ; IRQ6 - Floppy
    SET_IDT_ENTRY 39, irq_stub_table + 7 * 16,  0x8E   ; IRQ7 - LPT1
    SET_IDT_ENTRY 40, irq_stub_table + 8 * 16,  0x8E   ; IRQ8 - RTC
    SET_IDT_ENTRY 41, irq_stub_table + 9 * 16,  0x8E   ; IRQ9 - ACPI
    SET_IDT_ENTRY 42, irq_stub_table + 10 * 16, 0x8E   ; IRQ10
    SET_IDT_ENTRY 43, irq_stub_table + 11 * 16, 0x8E   ; IRQ11
    SET_IDT_ENTRY 44, irq_stub_table + 12 * 16, 0x8E   ; IRQ12 - Mouse
    SET_IDT_ENTRY 45, irq_stub_table + 13 * 16, 0x8E   ; IRQ13 - FPU
    SET_IDT_ENTRY 46, irq_stub_table + 14 * 16, 0x8E   ; IRQ14 - Primary ATA
    SET_IDT_ENTRY 47, irq_stub_table + 15 * 16, 0x8E   ; IRQ15 - Secondary ATA

    ; APIC Timer (vector 0xF0 = 240)
    SET_IDT_ENTRY 240, irq_stub_table + 16 * 16, 0x8E   ; APIC Timer

    ; Syscall (0x80): present, DPL=3, 64-bit interrupt gate (0xEE)
    ; Using interrupt gate for compatibility, but we'll use syscall instruction
    SET_IDT_ENTRY 128, syscall_entry, 0xEE  ; Syscall via int 0x80

    ret

; ============================================================
; ISR Stubs (generated by script or written manually)
; Each stub pushes error code (if applicable), vector number, then jumps to common handler
; ============================================================
section .text
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 32
    ; Check if this exception pushes error code (8, 10-14, 17)
    %if i == 8 || (i >= 10 && i <= 14) || i == 17
        push qword 0           ; Dummy error code for consistency
    %else
        push qword 0           ; No error code
    %endif
    push qword i               ; Vector number
    jmp isr_common
    times (16 - ($ - ($ - 16))) nop  ; Pad to 16 bytes
%assign i i + 1
%endrep

; Common ISR handler
extern isr_handler
isr_common:
    ; Save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Call C handler
    mov rdi, rsp             ; First arg = pointer to registers
    call isr_handler

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16              ; Pop error code and vector
    iretq

; ============================================================
; IRQ Stubs
; ============================================================
global irq_stub_table
irq_stub_table:
%assign i 0
%rep 16
    push qword 0             ; No error code for IRQs
    push qword (32 + i)      ; Vector number (32-47)
    jmp irq_common
    times (16 - ($ - ($ - 16))) nop
%assign i i + 1
%endrep

; APIC Timer stub (index 16, vector 240)
    push qword 0
    push qword 240
    jmp irq_common
    times (16 - ($ - ($ - 16))) nop

extern irq_handler
irq_common:
    ; Save registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Call C handler
    mov rdi, rsp
    call irq_handler

    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    cmp byte [rsp + 16 + 8], 40  ; Check vector (rsp+16=registers, +8=vector)
    jl .no_slave
    out 0xA0, al
.no_slave:

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

; ============================================================
; Syscall Entry (syscall instruction)
; ============================================================
global syscall_entry
syscall_entry:
    ; Syscall calling convention:
    ; RAX = syscall number
    ; RDI, RSI, RDX, R10, R8, R9 = args 1-6
    ; RCX = return RIP, R11 = return RFLAGS

    ; Save user registers
    push r11           ; RFLAGS
    push rcx           ; RIP
    push rax           ; Syscall number
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

    ; Switch to kernel stack
    swapgs
    mov [user_rsp], rsp
    mov rsp, [kernel_rsp]

    ; Save remaining registers
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Call syscall handler
    ; RAX = syscall number, RDI-R9 = args
    mov rdi, rax       ; syscall number
    mov rsi, [rsp + 8*8]   ; arg1 (was rdi)
    mov rdx, [rsp + 8*9]   ; arg2 (was rsi)
    mov rcx, [rsp + 8*10]  ; arg3 (was rdx)
    mov r8,  [rsp + 8*11]  ; arg4 (was r10)
    mov r9,  [rsp + 8*12]  ; arg5 (was r8)
    ; arg6 would be at [rsp + 8*13] (was r9)

    call syscall_handler

    ; Return value in RAX
    mov [rsp + 8*7], rax  ; Save return value where RAX was

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Switch back to user stack
    mov rsp, [user_rsp]
    swapgs

    ; Restore user registers
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rsi
    pop rdi
    pop rax           ; Return value
    pop rcx           ; RIP
    pop r11           ; RFLAGS

    sysretq

section .bss
global user_rsp, kernel_rsp
user_rsp:    resq 1
kernel_rsp:  resq 1