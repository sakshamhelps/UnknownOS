; Unknown OS - Paging Setup (4-level page tables)
; Author: byrex <byrex.me@proton.me>

bits 64
section .text
global vmm_init_early
extern pmm_alloc_page
extern kernel_physical_base
extern kernel_virtual_base

; ---------------------------------------------------------
; Early VMM init - called before kernel_main
; Sets up identity mapping + higher-half kernel mapping
; ---------------------------------------------------------
vmm_init_early:
    ; Allocate PML4 (Page Map Level 4)
    call pmm_alloc_page
    mov [pml4], rax
    mov rdi, rax
    call clear_page

    ; Identity map first 1 GiB (0x0000_0000_0000 - 0x0000_003F_FFFF)
    ; PML4[0] -> PDP
    call pmm_alloc_page
    mov [rax + 0*8], rdi
    or qword [rax + 0*8], 0x03        ; Present | Write
    mov rdi, rax
    call clear_page

    ; PDP[0] -> PD
    call pmm_alloc_page
    mov [rax + 0*8], rdi
    or qword [rax + 0*8], 0x03
    mov rdi, rax
    call clear_page

    ; PD[0..511] -> 2 MiB pages (identity map)
    mov rcx, 512
    mov r8, 0
.map_identity:
    call pmm_alloc_page
    mov [rdi + rcx*8 - 8], rax
    or qword [rdi + rcx*8 - 8], 0x83    ; Present | Write | Huge (2 MiB)
    loop .map_identity

    ; Higher-half kernel mapping (KERNEL_BASE = 0xFFFF800000000000)
    ; PML4[510] (0xFFFF800000000000) -> PDP
    call pmm_alloc_page
    mov [rax + 510*8], rdi
    or qword [rax + 510*8], 0x03
    mov rdi, rax
    call clear_page

    ; PDP[510] -> PD
    call pmm_alloc_page
    mov [rax + 510*8], rdi
    or qword [rax + 510*8], 0x03
    mov rdi, rax
    call clear_page

    ; PD[510] -> PT
    call pmm_alloc_page
    mov [rax + 510*8], rdi
    or qword [rax + 510*8], 0x03
    mov rdi, rax
    call clear_page

    ; Map kernel pages (4 KiB each)
    ; Kernel at physical kernel_physical_base, virtual kernel_virtual_base
    ; We'll map 2 MiB for now (covers kernel + initial heap)
    mov rsi, [rel kernel_physical_base]
    mov rdi, [rel kernel_virtual_base]
    sub rdi, 0xFFFF800000000000     ; Get offset into higher half
    shr rdi, 21                     ; PD index
    and rdi, 511

    mov rcx, 1                      ; Map 1 PD entry (2 MiB)
    call pmm_alloc_page
    mov [rax + rdi*8], rdx
    or qword [rax + rdi*8], 0x83    ; Present | Write | Huge

    ; Load CR3
    mov rax, [pml4]
    mov cr3, rax

    ; Enable 5-level paging if supported (CR4.LA57)
    ; For now, standard 4-level

    ret

; ---------------------------------------------------------
; Clear a page (4 KiB)
; RDI = physical address
; ---------------------------------------------------------
clear_page:
    push rcx
    push rdi
    mov rcx, 512
    xor rax, rax
    rep stosq
    pop rdi
    pop rcx
    ret

; ---------------------------------------------------------
; Map a 4 KiB page
; RDI = virtual address, RSI = physical address, RDX = flags
; ---------------------------------------------------------
global vmm_map_page_asm
vmm_map_page_asm:
    push rbx
    push rbp
    push r12
    push r13

    mov rbx, [pml4]

    ; PML4 index (bits 47-39)
    mov rbp, rdi
    shr rbp, 39
    and rbp, 511

    mov r12, [rbx + rbp*8]
    test r12, 1
    jnz .pdp_exists
    call pmm_alloc_page
    mov r12, rax
    call clear_page
    mov [rbx + rbp*8], r12
    or qword [rbx + rbp*8], 0x03
.pdp_exists:
    and r12, ~0xFFF

    ; PDP index (bits 38-30)
    mov rbp, rdi
    shr rbp, 30
    and rbp, 511

    mov r13, [r12 + rbp*8]
    test r13, 1
    jnz .pd_exists
    call pmm_alloc_page
    mov r13, rax
    call clear_page
    mov [r12 + rbp*8], r13
    or qword [r12 + rbp*8], 0x03
.pd_exists:
    and r13, ~0xFFF

    ; PD index (bits 29-21)
    mov rbp, rdi
    shr rbp, 21
    and rbp, 511

    mov r12, [r13 + rbp*8]
    test r12, 1
    jnz .pt_exists
    call pmm_alloc_page
    mov r12, rax
    call clear_page
    mov [r13 + rbp*8], r12
    or qword [r13 + rbp*8], 0x03
.pt_exists:
    and r12, ~0xFFF

    ; PT index (bits 20-12)
    mov rbp, rdi
    shr rbp, 12
    and rbp, 511

    mov [r12 + rbp*8], rsi
    or qword [r12 + rbp*8], rdx

    ; Invalidate TLB for this page
    invlpg [rdi]

    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

; ---------------------------------------------------------
; Unmap a page
; RDI = virtual address
; ---------------------------------------------------------
global vmm_unmap_page_asm
vmm_unmap_page_asm:
    push rbx
    push rbp
    push r12
    push r13

    mov rbx, [pml4]

    ; PML4 index
    mov rbp, rdi
    shr rbp, 39
    and rbp, 511
    mov r12, [rbx + rbp*8]
    test r12, 1
    jz .done
    and r12, ~0xFFF

    ; PDP index
    mov rbp, rdi
    shr rbp, 30
    and rbp, 511
    mov r13, [r12 + rbp*8]
    test r13, 1
    jz .done
    and r13, ~0xFFF

    ; PD index
    mov rbp, rdi
    shr rbp, 21
    and rbp, 511
    mov r12, [r13 + rbp*8]
    test r12, 1
    jz .done
    and r12, ~0xFFF

    ; PT index
    mov rbp, rdi
    shr rbp, 12
    and rbp, 511
    mov qword [r12 + rbp*8], 0

    invlpg [rdi]

.done:
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

section .bss
align 4096
pml4: resq 1