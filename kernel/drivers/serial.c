/*
 * Unknown OS - Serial Driver (UART 16550)
 * COM1: 0x3F8, IRQ 4
 * Author: byrex <byrex.me@proton.me>
 */

#include <kernel.h>

#define COM1 0x3F8

/* UART Registers */
#define UART_RBR  0  /* Receive Buffer Register (read) */
#define UART_THR  0  /* Transmitter Holding Register (write) */
#define UART_DLL  0  /* Divisor Latch Low (DLAB=1) */
#define UART_IER  1  /* Interrupt Enable Register */
#define UART_DLM  1  /* Divisor Latch High (DLAB=1) */
#define UART_IIR  2  /* Interrupt Identification Register (read) */
#define UART_FCR  2  /* FIFO Control Register (write) */
#define UART_LCR  3  /* Line Control Register */
#define UART_MCR  4  /* Modem Control Register */
#define UART_LSR  5  /* Line Status Register */
#define UART_MSR  6  /* Modem Status Register */
#define UART_SCR  7  /* Scratch Register */

/* Line Status Register bits */
#define LSR_DR    0x01  /* Data Ready */
#define LSR_OE    0x02  /* Overrun Error */
#define LSR_PE    0x04  /* Parity Error */
#define LSR_FE    0x08  /* Framing Error */
#define LSR_BI    0x10  /* Break Interrupt */
#define LSR_THRE  0x20  /* THR Empty */
#define LSR_TEMT  0x40  /* Transmitter Empty */
#define LSR_FIFOE 0x80  /* FIFO Error */

/* ============================================================
 * Serial Initialization
 * ============================================================ */
void serial_init(void) {
    /* Disable interrupts */
    outb(COM1 + UART_IER, 0x00);

    /* Set DLAB=1 to program baud rate */
    outb(COM1 + UART_LCR, 0x80);

    /* 115200 baud: divisor = 115200 / 115200 = 1 */
    outb(COM1 + UART_DLL, 0x01);
    outb(COM1 + UART_DLM, 0x00);

    /* 8 bits, no parity, 1 stop bit, DLAB=0 */
    outb(COM1 + UART_LCR, 0x03);

    /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1 + UART_FCR, 0xC7);

    /* Enable RTS/DTR, enable interrupts */
    outb(COM1 + UART_MCR, 0x0B);

    /* Enable receive interrupt */
    outb(COM1 + UART_IER, 0x01);

    serial_puts("[SERIAL] COM1 initialized at 115200 8N1\n");
}

/* ============================================================
 * Output Functions
 * ============================================================ */
void serial_putc(char c) {
    /* Wait for THR empty */
    while (!(inb(COM1 + UART_LSR) & LSR_THRE)) {
        asm volatile ("pause");
    }
    outb(COM1 + UART_THR, c);
}

void serial_puts(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

void serial_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    serial_puts(buf);
}

/* ============================================================
 * Input Functions
 * ============================================================ */
bool serial_has_char(void) {
    return inb(COM1 + UART_LSR) & LSR_DR;
}

char serial_getc(void) {
    while (!serial_has_char()) {
        asm volatile ("pause");
    }
    return inb(COM1 + UART_RBR);
}