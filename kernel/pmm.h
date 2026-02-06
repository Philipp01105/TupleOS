/*
 * Physical Memory Manager (PMM) interface.
 *
 * This module is responsible for managing physical memory at the page-frame
 * level. It uses the memory map provided by a Multiboot-compliant bootloader
 * to track which physical memory regions are available and which are reserved.
 *
 * The PMM operates on fixed-size page frames (PAGE_SIZE, typically 4 KB) and
 * provides basic allocation and deallocation primitives used during early
 * kernel initialization and by higher-level memory managers (e.g. the virtual
 * memory manager).
 *
 * Responsibilities:
 * - Initialize physical memory bookkeeping using the Multiboot memory map
 * - Track total and free physical memory
 * - Allocate and free individual physical page frames
 *
 * Design notes:
 * - All addresses returned by the PMM are physical addresses.
 * - The PMM does not handle virtual addressing or paging; it is intended to
 *   serve as a low-level allocator for systems such as the VMM or kernel heap.
 * - PAGE_SIZE is fixed and must match the paging configuration of the system.
*/


#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

// Page size in bytes
#define PAGE_SIZE 4096

// init physical memory manager using multiboot memory map
void pmm_init(multiboot_info_t* mbi);

// allocate a single 4KB page frame, returns physical addr or NULL if out of memory
void* pmm_alloc_frame(void);

// Free a previously allocated page frame
void pmm_free_frame(void* frame);

// get total physical meory in bytes
uint32_t pmm_get_total_memory(void);

// get free physical memory in bytes
uint32_t pmm_get_free_memory(void);

#endif