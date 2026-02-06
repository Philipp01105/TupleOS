#include "kprintf.h"
#include "terminal.h"
#include <stdarg.h>

// krpintf("Value: %d, Name: %s", 42, "aaron")

static void print_uint(uint32_t num) {
    if (num == 0) {
        terminal_putchar('0');
        return;
    }
    char buffer[12];
    int i = 0;
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (--i >= 0) {
        terminal_putchar(buffer[i]);
    }
}

static void print_int(int32_t num) {
    if (num < 0) {
        terminal_putchar('-');
        num = -num;
    }
    print_uint((uint32_t)num);
}

static void print_hex(uint32_t num) {
    if (num == 0) {
        terminal_putchar('0');
        return;
    }
    char buffer[8];
    int i = 0;
    while (num > 0) {
        uint8_t digit = num & 0xF;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        num >>= 4;
    }
    while (--i >= 0) {
        terminal_putchar(buffer[i]);
    }
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    while (*format) {
        if (*format == '%') {
            format++; // skip %
            switch (*format) {
                case 'd': {
                    print_int(va_arg(args, uint32_t));
                    break;
                }
                case 'u': {
                    print_uint(va_arg(args, uint32_t));
                    break;
                }
                case 'x': {
                    print_hex(va_arg(args, uint32_t));
                    break;
                }
                case 's': {
                    terminal_writestring(va_arg(args, char*));
                    break;
                }
                case '%': {
                    terminal_putchar('%');
                    terminal_putchar(*format);
                    break;
                }
            }
        } else {
            terminal_putchar(*format);
        }
        format++;
    }
    va_end(args);
}
