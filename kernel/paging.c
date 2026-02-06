#include "paging.h"
#include "pmm.h"
#include "kprintf.h"

// Page dir, must be 4KB aligned
// Contains 1024 entires, each pointing to a page table
static page_dir_entry_t page_directory[PAGE_ENTRIES] __attribute__((aligned(4096)));

// Page tables for first 16MB (4 tablex x 4MB each)
static page_table_entry_t page_tables[4][PAGE_ENTRIES] __attribute__((aligned(4096)));

// Pointer to curr page dir (physical addr)
static page_dir_entry_t* current_page_directory = NULL;

// Enable paging by setting CRO bit 31
static void paging_enable(void) {
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // Set PG bit (bit 31)
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}

// Load page dir address into CR3
static void paging_load_directory(page_dir_entry_t* dir) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(dir));
}

void paging_flush_tlb(uint32_t virtual_addr) {
    __asm__ volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

void paging_init(void) {
    kprintf("Paging: Initializing...\n");
    // clear the page dir
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        // Not present, read/write, supervisor only
        page_directory[i] = 0x00000002; // R/W but not present
    }

    // Identity map first 16MB (4 page tables, 4MB each)
    for (int t = 0; t < 4; t++) {
        for (int i = 0; i < PAGE_ENTRIES; i++) {
            uint32_t addr = (t * PAGE_ENTRIES + i) * PAGE_SIZE;
            page_tables[t][i] = addr | PAGE_PRESENT | PAGE_WRITE;
        }
        page_directory[t] = ((uint32_t)page_tables[t]) | PAGE_PRESENT | PAGE_WRITE;
    }


    // Store current page dir
    current_page_directory = page_directory;

    // Load page directly into CR3
    paging_load_directory(page_directory);

    // Enable paging
    paging_enable();

    kprintf("Paging: Enabled, first 16MB identity mapped\n");
}

void paging_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    // Get page dir index (top 10 bits)
    uint32_t pd_index = virtual_addr >> 22;
    // Get page table index (middle 10 bits)
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    // Check if table exists
    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        // Allocate a new page table
        page_table_entry_t* new_table = (page_table_entry_t*)pmm_alloc_frame();
        if (!new_table) {
            kprintf("Paging: Failed to allocate page table\n");
            return;
        }

        // Clear the new page table
        for (int i = 0; i < PAGE_ENTRIES; i++) {
            new_table[i] = 0;
        }

        // Add to page dir
        page_directory[pd_index] = ((uint32_t)new_table) | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    // Get the table
    page_table_entry_t* table = (page_table_entry_t*)(page_directory[pd_index] & 0xFFFFF000);

    // Map the page
    table[pt_index] = (physical_addr & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;

    // Flush TLB for this address
    paging_flush_tlb(virtual_addr);
}

void paging_unmap_page(uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        return; // Table doesn't exist
    }

    page_table_entry_t* table = (page_table_entry_t*)(page_directory[pd_index] & 0xFFFFF000);
    table[pt_index] = 0; // Clear entry

    paging_flush_tlb(virtual_addr);
}

uint32_t paging_get_physical(uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    uint32_t offset = virtual_addr & 0xFFF;

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        return 0;
    }

    page_table_entry_t* table = (page_table_entry_t*)(page_directory[pd_index] & 0xFFFFF000);

    if (!(table[pt_index] & PAGE_PRESENT)) {
        return 0;
    }

    return (table[pt_index] & 0xFFFFF000) + offset;
}