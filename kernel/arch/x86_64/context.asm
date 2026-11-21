; Unknown OS - Context Switch
; Author: byrex <byrex.me@proton.me>

bits 64
section .text
global context_switch

; void context_switch(struct task_context *old, struct task_context *new)
; RDI = old context, RSI = new context
context_switch:
    ; Save old context
    mov [rdi + 0x00], rax
    mov [rdi + 0x08], rbx
    mov [rdi + 0x10], rcx
    mov [rdi + 0x18], rdx
    mov [rdi + 0x20], rsi
    mov [rdi + 0x28], rdi
    mov [rdi + 0x30], rbp
    mov [rdi + 0x38], r8
    mov [rdi + 0x40], r9
    mov [rdi + 0x48], r10
    mov [rdi + 0x50], r11
    mov [rdi + 0x58], r12
    mov [rdi + 0x60], r13
    mov [rdi + 0x68], r14
    mov [rdi + 0x70], r15

    ; Save RSP and RFLAGS
    mov [rdi + 0x78], rsp
    pushfq
    pop rax
    mov [rdi + 0x80], rax

    ; Save RIP (return address)
    mov rax, [rsp]
    mov [rdi + 0x88], rax

    ; Load new context
    mov rsp, [rsi + 0x78]

    mov rax, [rsi + 0x80]
    push rax
    popfq

    mov rax, [rsi + 0x00]
    mov rbx, [rsi + 0x08]
    mov rcx, [rsi + 0x10]
    mov rdx, [rsi + 0x18]
    mov rsi, [rsi + 0x20]
    mov rdi, [rsi + 0x28]
    mov rbp, [rsi + 0x30]
    mov r8,  [rsi + 0x38]
    mov r9,  [rsi + 0x40]
    mov r10, [rsi + 0x48]
    mov r11, [rsi + 0x50]
    mov r12, [rsi + 0x58]
    mov r13, [rsi + 0x60]
    mov r14, [rsi + 0x68]
    mov r15, [rsi + 0x70]

    ; Push new RIP and return
    push qword [rsi + 0x88]
    ret