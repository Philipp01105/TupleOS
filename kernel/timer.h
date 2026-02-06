#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/*
* The PIT is a chip that fires IRQ 0 at a configurable frequency
* By default it fires at 18.2 Hz, (18.2 times per second) which is too slow for most purposes
* We can reprogram it to fire at a higher rate (e.g. 100 Hz) for better timer resolution
*/

#define TIMER_IRQ 32

void timer_init(uint32_t frequency);

// Get number of ticks since boot
uint32_t timer_get_ticks(void);

#endif