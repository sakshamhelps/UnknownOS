/*
 * Unknown OS - ISR Handlers
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * Exception Names
 * ============================================================ */
static const char *exception_names[32] = {
    "#DE Divide Error",
    "#DB Debug",
    "NMI Non-Maskable Interrupt",
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack Segment Fault",
    "#GP General Protection Fault",
    "#PF Page Fault",
    "Reserved",
    "#MF x87 FPU",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD FPU",
    "#VE Virtualization",
    "#CP Control Protection",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved"
};

/* ============================================================
 * ISR Handler
 * Called from assembly with registers on stack
 * ============================================================ */
struct isr_regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t error_code, vector;
};

void isr_handler(struct isr_regs *regs) {
    uint64_t vector = regs->vector;

    if (vector < 32) {
        serial_printf("[EXCEPTION] %s (vector %lu)\n", exception_names[vector], vector);
        serial_printf("  RIP=0x%lx RSP=0x%lx RFLAGS=0x%lx\n", regs->rcx, regs->rsp, regs->r11);
        serial_printf("  RAX=0x%lx RBX=0x%lx RCX=0x%lx RDX=0x%lx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        serial_printf("  RSI=0x%lx RDI=0x%lx RBP=0x%lx\n", regs->rsi, regs->rdi, regs->rbp);
        serial_printf("  R8=0x%lx R9=0x%lx R10=0x%lx R11=0x%lx\n", regs->r8, regs->r9, regs->r10, regs->r11);
        serial_printf("  R12=0x%lx R13=0x%lx R14=0x%lx R15=0x%lx\n", regs->r12, regs->r13, regs->r14, regs->r15);
        serial_printf("  Error Code=0x%lx\n", regs->error_code);

        /* Page fault - print CR2 */
        if (vector == 14) {
            uint64_t cr2;
            asm volatile ("mov %%cr2, %0" : "=r"(cr2));
            serial_printf("  CR2 (fault address)=0x%lx\n", cr2);

            /* Check if it's a user-mode fault */
            if (regs->error_code & 0x4) {
                serial_puts("  User-mode page fault\n");
            }
        }

        /* Double fault - panic */
        if (vector == 8) {
            serial_puts("[KERNEL] Double fault - halting\n");
            for (;;) asm volatile ("hlt");
        }

        /* General protection fault */
        if (vector == 13) {
            serial_puts("[KERNEL] General protection fault\n");
        }

        /* Invalid opcode - might be syscall attempt */
        if (vector == 6) {
            serial_puts("[KERNEL] Invalid opcode\n");
        }
    }

    /* For now, halt on any exception */
    if (vector < 32) {
        serial_puts("[KERNEL] Unhandled exception - halting\n");
        for (;;) asm volatile ("hlt");
    }
}