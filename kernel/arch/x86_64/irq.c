/*
 * Unknown OS - IRQ Handlers
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * IRQ Handler
 * ============================================================ */
struct isr_regs {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t error_code, vector;
};

/* IRQ handler function pointers */
typedef void (*irq_handler_t)(struct isr_regs *regs);
static irq_handler_t irq_handlers[16] = {0};

void irq_handler(struct isr_regs *regs) {
    uint64_t vector = regs->vector;

    if (vector >= 32 && vector < 48) {
        uint8_t irq = vector - 32;

        if (irq_handlers[irq]) {
            irq_handlers[irq](regs);
        } else {
            /* Spurious or unhandled IRQ */
            if (irq != 7 && irq != 15) {  /* Ignore spurious IRQ7/15 */
                serial_printf("[IRQ] Unhandled IRQ %d\n", irq);
            }
        }
    }
}

/* ============================================================
 * IRQ Registration
 * ============================================================ */
void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

void irq_unregister_handler(uint8_t irq) {
    if (irq < 16) {
        irq_handlers[irq] = NULL;
    }
}

/* ============================================================
 * Timer IRQ (IRQ0 / Vector 32 via APIC)
 * ============================================================ */
static uint64_t timer_ticks = 0;

void timer_irq_handler(struct isr_regs *regs) {
    (void)regs;
    timer_ticks++;

    /* Send EOI to local APIC */
    extern void lapic_send_eoi(void);
    lapic_send_eoi();

    /* Call scheduler tick */
    extern void sched_timer_tick(void);
    sched_timer_tick();
}

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_sleep(uint64_t ms) {
    uint64_t start = timer_ticks;
    while (timer_ticks - start < ms / 10) {
        asm volatile ("hlt");
    }
}

/* ============================================================
 * Keyboard IRQ (IRQ1) - Vector 33 via IOAPIC
 * ============================================================ */
void keyboard_irq_handler(struct isr_regs *regs) {
    (void)regs;
    extern void ps2_interrupt_handler(void);
    ps2_interrupt_handler();

    /* Send EOI to local APIC */
    extern void lapic_send_eoi(void);
    lapic_send_eoi();
}

/* ============================================================
 * PIC Initialization (legacy, disabled when using APIC)
 * ============================================================ */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

#define PIC_EOI     0x20

#define ICW1_ICW4       0x01
#define ICW1_SINGLE     0x02
#define ICW1_INTERVAL4  0x04
#define ICW1_LEVEL      0x08
#define ICW1_INIT       0x10

#define ICW4_8086       0x01
#define ICW4_AUTO       0x02
#define ICW4_BUF_SLAVE  0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM       0x10

void pic_init(void) {
    serial_puts("[PIC] Disabling legacy PIC (using APIC)...\n");

    /* Mask all interrupts on PIC */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    serial_puts("[PIC] Disabled\n");
}

void pic_mask_irq(uint8_t irq) {
    (void)irq;  /* No-op when using APIC */
}

void pic_unmask_irq(uint8_t irq) {
    (void)irq;  /* No-op when using APIC */
}

void pic_send_eoi(uint8_t irq) {
    (void)irq;  /* No-op when using APIC */
}

/* ============================================================
 * APIC IRQ Registration (vectors 32-255)
 * ============================================================ */
#define MAX_IRQ_HANDLERS 256
static irq_handler_t irq_handlers[MAX_IRQ_HANDLERS] = {0};

void apic_register_irq_handler(uint8_t vector, irq_handler_t handler) {
    if (vector < MAX_IRQ_HANDLERS) {
        irq_handlers[vector] = handler;
    }
}

void apic_unregister_irq_handler(uint8_t vector) {
    if (vector < MAX_IRQ_HANDLERS) {
        irq_handlers[vector] = NULL;
    }
}

void irq_handler(struct isr_regs *regs) {
    uint64_t vector = regs->vector;

    if (vector < MAX_IRQ_HANDLERS && irq_handlers[vector]) {
        irq_handlers[vector](regs);
    } else {
        /* Spurious or unhandled IRQ */
        if (vector != 0xFF && vector != 39 && vector != 47) {  /* Ignore common spurious */
            serial_printf("[IRQ] Unhandled vector %lu\n", vector);
        }
    }
}