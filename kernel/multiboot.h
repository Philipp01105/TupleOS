/*
 * Definitions for interacting with Multiboot-compliant bootloaders (e.g. GRUB).
 *
 * This header declares constants, flags, and data structures defined by the
 * Multiboot specification. These are used by the kernel to interpret the
 * information structure provided by the bootloader at startup, including
 * system memory layout and boot parameters.
 *
 * Key components:
 * - MULTIBOOT_MAGIC: Magic value used to verify a valid Multiboot environment.
 * - Multiboot info flags: Indicate which fields in the multiboot_info_t
 *   structure are valid and safe to read.
 * - Memory type constants: Describe the usability of physical memory regions
 *   as reported by the bootloader.
 * - multiboot_info_t: Primary structure populated by the bootloader and passed
 *   to the kernel, containing memory size, memory maps, modules, and other
 *   boot-time metadata.
 * - multiboot_mmap_entry_t: Describes individual entries in the physical memory
 *   map used by the kernel to determine available and reserved memory regions.
 *
 * All structures are packed to ensure binary compatibility with the Multiboot
 * specification and must be accessed using physical addresses during early
 * boot before paging is fully established.
*/

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

// Multi boot magic number
#define MULTIBOOT_MAGIC 0x2BADB002

// Multiboot info flags
#define MULTIBOOT_FLAG_MEM (1 << 0) // mem_lower and mem_upper are valid
#define MULTIBOOT_FLAG_MMAP (1 << 6) // mmap* fields are valid

// Memory map entry types
#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS 4
#define MULTIBOOT_MEMORY_BADRAM 5

// Multiboot information structure passed by GRUB
typedef struct {
    uint32_t flags; // which fields are valid
    uint32_t mem_lower; // KB of lower memory (below 1MB)
    uint32_t mem_upper; // KB of upper memory (above 1MB)
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length; // total size of memory map buffer
    uint32_t mmap_addr; // physical address of memory map
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} __attribute__((packed)) multiboot_info_t;

// Memory map entry structure
// Note: size field is NOT included in size value itself
typedef struct {
    uint32_t size; // size of this entry (not including this field)
    uint64_t base_addr; // physical address of memory region
    uint64_t length; // length of memory region in bytes
    uint32_t type; // type (1 == available, 2+ = reserved)
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif