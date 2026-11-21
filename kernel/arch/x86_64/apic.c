/*
 * Unknown OS - APIC/IOAPIC Driver
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

/* ============================================================
 * APIC Registers
 * ============================================================ */
#define APIC_BASE_MSR     0x1B
#define APIC_ENABLE       (1 << 11)

/* Local APIC registers (offsets from base) */
#define APIC_ID           0x020
#define APIC_VERSION      0x030
#define APIC_TPR          0x080
#define APIC_APR          0x090
#define APIC_PPR          0x0A0
#define APIC_EOI          0x0B0
#define APIC_LDR          0x0D0
#define APIC_DFR          0x0E0
#define APIC_SVR          0x0F0
#define APIC_ISR          0x100
#define APIC_TMR          0x180
#define APIC_IRR          0x200
#define APIC_ESR          0x280
#define APIC_ICR_LOW      0x300
#define APIC_ICR_HIGH     0x310
#define APIC_LVT_TIMER    0x320
#define APIC_LVT_THERMAL  0x330
#define APIC_LVT_PMC      0x340
#define APIC_LVT_LINT0    0x350
#define APIC_LVT_LINT1    0x360
#define APIC_LVT_ERROR    0x370
#define APIC_TIMER_ICR    0x380
#define APIC_TIMER_CCR    0x390
#define APIC_TIMER_DCR    0x3E0

/* SVR bits */
#define APIC_SVR_ENABLE   (1 << 8)

/* LVT bits */
#define APIC_LVT_MASK     (1 << 16)
#define APIC_LVT_TIMER_PERIODIC (1 << 17)
#define APIC_LVT_DELIVERY_FIXED 0
#define APIC_LVT_DELIVERY_NMI   4

/* ICR bits */
#define APIC_ICR_DELIVERY_FIXED  0
#define APIC_ICR_DELIVERY_NMI    4
#define APIC_ICR_DEST_SELF       0x40000
#define APIC_ICR_DEST_ALL_INC    0x80000
#define APIC_ICR_DEST_ALL_EXC    0xC0000

/* ============================================================
 * IOAPIC
 * ============================================================ */
#define IOAPIC_BASE       0xFEC00000
#define IOAPIC_REGSEL     0x00
#define IOAPIC_WINDOW     0x10

#define IOAPIC_ID         0x00
#define IOAPIC_VERSION    0x01
#define IOAPIC_ARB        0x02
#define IOAPIC_REDTBL     0x10  /* 24 registers (2 per interrupt) */

static uintptr_t lapic_base = 0;
static uintptr_t ioapic_base = 0;

/* ============================================================
 * MSR Access
 * ============================================================ */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    asm volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    asm volatile ("wrmsr" :: "c"(msr), "a"((uint32_t)value), "d"(value >> 32));
}

/* ============================================================
 * Local APIC Access
 * ============================================================ */
static inline uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *)(lapic_base + reg);
}

static inline void lapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(lapic_base + reg) = value;
}

/* ============================================================
 * IOAPIC Access
 * ============================================================ */
static inline uint32_t ioapic_read(uint8_t reg) {
    *(volatile uint32_t *)(ioapic_base + IOAPIC_REGSEL) = reg;
    return *(volatile uint32_t *)(ioapic_base + IOAPIC_WINDOW);
}

static inline void ioapic_write(uint8_t reg, uint32_t value) {
    *(volatile uint32_t *)(ioapic_base + IOAPIC_REGSEL) = reg;
    *(volatile uint32_t *)(ioapic_base + IOAPIC_WINDOW) = value;
}

/* ============================================================
 * Local APIC Initialization
 * ============================================================ */
void lapic_init(void) {
    serial_puts "[LAPIC] Initializing...\n");

    /* Get APIC base from MSR */
    uint64_t msr = rdmsr(APIC_BASE_MSR);
    lapic_base = msr & ~0xFFF;
    serial_printf("[LAPIC] Base address: 0x%lx\n", lapic_base);

    /* Enable APIC */
    wrmsr(APIC_BASE_MSR, msr | APIC_ENABLE);

    /* Enable APIC in SVR */
    lapic_write(APIC_SVR, lapic_read(APIC_SVR) | APIC_SVR_ENABLE);

    /* Set spurious vector (0xFF) */
    lapic_write(APIC_SVR, (lapic_read(APIC_SVR) & ~0xFF) | 0xFF);

    /* Disable LINT0/LINT1 (we use IOAPIC) */
    lapic_write(APIC_LVT_LINT0, APIC_LVT_MASK);
    lapic_write(APIC_LVT_LINT1, APIC_LVT_MASK);

    /* Disable error LVT */
    lapic_write(APIC_LVT_ERROR, APIC_LVT_MASK);

    /* Set up timer (one-shot, vector 0xF0) */
    lapic_write(APIC_LVT_TIMER, 0xF0 | APIC_LVT_MASK);
    lapic_write(APIC_TIMER_DCR, 0x3);  /* Divide by 16 */
    lapic_write(APIC_TIMER_ICR, 0);    /* Disabled for now */

    serial_puts("[LAPIC] Initialized\n");
}

/* ============================================================
 * IOAPIC Initialization
 * ============================================================ */
void ioapic_init(void) {
    serial_puts("[IOAPIC] Initializing...\n");

    ioapic_base = IOAPIC_BASE;

    uint32_t version = ioapic_read(IOAPIC_VERSION);
    uint8_t max_redir = (version >> 16) & 0xFF;
    serial_printf("[IOAPIC] Version: 0x%x, Max Redirection Entries: %d\n",
                  version & 0xFF, max_redir + 1);

    /* Mask all interrupts initially */
    for (uint8_t i = 0; i <= max_redir; i++) {
        ioapic_write(IOAPIC_REDTBL + i * 2, 0x00010000);  /* Masked */
        ioapic_write(IOAPIC_REDTBL + i * 2 + 1, 0);       /* Destination */
    }

    /* Map IRQs to vectors (32-47 for ISA) */
    for (uint8_t irq = 0; irq <= max_redir; irq++) {
        uint32_t vec = 32 + irq;
        uint32_t low = vec | (1 << 16);  /* Masked, fixed delivery */
        uint32_t high = 0;  /* CPU 0 */

        ioapic_write(IOAPIC_REDTBL + irq * 2, low);
        ioapic_write(IOAPIC_REDTBL + irq * 2 + 1, high);
    }

    /* Unmask keyboard (IRQ1) */
    ioapic_write(IOAPIC_REDTBL + 1 * 2, 33);       /* Vector 33, unmasked */

    /* Timer is handled by local APIC timer (vector 0xF0) - not via IOAPIC */

    serial_puts("[IOAPIC] Initialized\n");
}

/* ============================================================
 * IPI Functions
 * ============================================================ */
void lapic_send_ipi(uint8_t vector, uint32_t destination) {
    lapic_write(APIC_ICR_HIGH, destination << 24);
    lapic_write(APIC_ICR_LOW, vector | APIC_ICR_DELIVERY_FIXED);

    /* Wait for delivery */
    while (lapic_read(APIC_ICR_LOW) & (1 << 12)) {
        asm volatile ("pause");
    }
}

void lapic_send_eoi(void) {
    lapic_write(APIC_EOI, 0);
}

/* ============================================================
 * Timer Functions
 * ============================================================ */
void lapic_timer_init(uint32_t frequency_hz) {
    /* Calibrate using PIT */
    serial_printf("[LAPIC] Calibrating timer for %u Hz...\n", frequency_hz);

    /* Use PIT to calibrate APIC timer */
    /* PIT channel 0, mode 2 (rate generator), binary count */
    outb(0x43, 0x34);
    outb(0x40, 0xFF);
    outb(0x40, 0xFF);

    /* Start APIC timer in one-shot mode at max count */
    lapic_write(APIC_TIMER_DCR, 0x3);  /* Divide by 16 */
    lapic_write(APIC_LVT_TIMER, 0xF0 | APIC_LVT_MASK);  /* Masked, vector 0xF0 */
    lapic_write(APIC_TIMER_ICR, 0xFFFFFFFF);

    /* Wait for PIT to wrap around (about 54.9ms at 18.2Hz) */
    /* Actually measure 10ms using PIT */
    uint64_t start_pit = 0;
    inb(0x40);  /* Latch low byte */
    start_pit = inb(0x40);
    start_pit |= (uint64_t)inb(0x40) << 8;

    /* Wait ~10ms */
    for (volatile int i = 0; i < 1000000; i++) {
        asm volatile ("pause");
    }

    uint64_t end_pit = 0;
    inb(0x40);
    end_pit = inb(0x40);
    end_pit |= (uint64_t)inb(0x40) << 8;

    uint64_t pit_delta = start_pit - end_pit;  /* PIT counts down */
    uint64_t apic_delta = 0xFFFFFFFF - lapic_read(APIC_TIMER_CCR);

    /* Calculate ticks per second */
    uint64_t ticks_per_10ms = apic_delta;
    uint64_t ticks_per_sec = ticks_per_10ms * 100;

    serial_printf("[LAPIC] Calibrated: %lu ticks/sec\n", ticks_per_sec);

    /* Now set up periodic timer */
    uint32_t icr = (uint32_t)(ticks_per_sec / frequency_hz);
    lapic_write(APIC_TIMER_DCR, 0x3);  /* Divide by 16 */
    lapic_write(APIC_LVT_TIMER, 0xF0 | APIC_LVT_TIMER_PERIODIC);  /* Vector 0xF0, periodic */
    lapic_write(APIC_TIMER_ICR, icr);

    serial_printf("[LAPIC] Timer initialized with ICR=%u\n", icr);
}

void lapic_timer_set_oneshot(uint32_t ticks) {
    lapic_write(APIC_LVT_TIMER, 0xF0);  /* One-shot, vector 0xF0 */
    lapic_write(APIC_TIMER_ICR, ticks);
}

/* ============================================================
 * APIC Timer Handler
 * ============================================================ */
extern void timer_irq_handler(struct isr_regs *regs);

/* ============================================================
 * APIC Init (called from kernel_main)
 * ============================================================ */
void apic_init(void) {
    lapic_init();
    ioapic_init();

    /* Register APIC timer handler (vector 0xF0 = 240) */
    apic_register_irq_handler(240, timer_irq_handler);

    serial_puts("[APIC] Timer handler registered\n");
}