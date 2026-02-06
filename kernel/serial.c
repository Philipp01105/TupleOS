#include "serial.h"
#include "ports.h"
#include <stdarg.h>

// Check if transmit buffer is empty
static int serial_is_transmit_empty() {
    return inb(COM1 + 5) & 0x20; // Checking to see if transmit buffer is empty (bit 5)
}

void serial_init(void) {
    outb(COM1 + 1, 0x00); // Disable all interrupts (we will poll instead)

    outb(COM1 + 3, 0x80); // Enable DLAB (set baud rate divisor)
                          // This changes what ports 0 and 1 mean temporarily
                          // Now port 0 = divisor low byte, port 1 = divisor high byte

    // Set baud rate via division
    outb(COM1 + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00); //                  (hi byte)

    // DLAB=0, and set: 8 data bits, no parity, one stop bit
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop bit

    outb(COM1 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold

    outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void serial_putchar(char c) {
    while (serial_is_transmit_empty() == 0);
    outb(COM1, c);
}

void serial_writestring(const char* str) {
    while (*str) {
        serial_putchar(*str++);
    }
}

// Helper functions for serial_printf
static void serial_print_uint(uint32_t num) {
    if (num == 0) { serial_putchar('0'); return; }
    char buffer[12];
    int i =0;
    while (num > 0) { buffer[i++] = '0' + (num % 10); num /= 10; }
    while (--i >= 0) serial_putchar(buffer[i]);
}

static void serial_print_int(int32_t num) {
    if (num < 0) { serial_putchar('-'); num = -num; }
    serial_print_uint((uint32_t)num);
}

static void serial_print_hex(uint32_t num) {
    if (num == 0) { serial_putchar('0'); return; }
    char buffer[8];
    int i = 0;
    while (num > 0) {
        uint8_t digit = num & 0xF;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        num >>= 4;
    }
    while (--i >= 0) serial_putchar(buffer[i]);
}

void serial_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++; // skip %
            switch (*format) {
                case 'd': { serial_print_int(va_arg(args, int32_t)); break; }
                case 'u': { serial_print_uint(va_arg(args, uint32_t)); break; }
                case 'x': { serial_print_hex(va_arg(args, uint32_t)); break; }
                case 's': { serial_writestring(va_arg(args, const char*)); break; }
                case '%': { serial_putchar('%'); break; }
                default: { serial_putchar('%'); serial_putchar(*format); break; }
            }
        } else {
            serial_putchar(*format);
        }
        format++;
    }

    va_end(args);
}