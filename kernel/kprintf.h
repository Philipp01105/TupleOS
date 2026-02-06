#ifndef KPRINTF_H
#define KPRINTF_H

// Kernel prinf, formatted output to terminal!
// Supports: %d (int), %u (unsigned), %x (hex), %s (string), %% (literal %)
void kprintf(const char* format, ...);

#endif