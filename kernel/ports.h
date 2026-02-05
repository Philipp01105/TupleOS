// Tiny utility header, almost everything in an OS talks to hardware through IO ports
// two x86 instructions: inb (read a byte from a port) and outb (write a byte to a port)
// Every file after this will use these
#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

/*
* On x86 the CPU talks to hardware (keyboard, PIC, timer, etc.) through IO ports, separate address space from RAM
* Each device lives at a specific port number

* These are "inline" so the compiler inserts the assembly directly wherever they're called, avoiding function call overhead
* This matters because port IO happens extremely frequently
*/

/* Write a byte to an IO port
* "a" constraint = value must be in the AL register (x86 requirement)
* "Nd" constraint = port must be a constant or in the DX register
*/
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* Read a byte from an I/O port
 * "=a" means the result comes back in the AL register
 */
 static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
 }

 /* Sometimes you need a tiny delay between port operations
 * Writing to port 0x80 (unused "POST diagnostic" port) wastes just enough time for slow hardware to catch up
 */
 static inline void io_wait(void) {
    outb(0x80, 0);
 }

 #endif