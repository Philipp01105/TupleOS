#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>

void kheap_init(void);

void* kmalloc(size_t size);

void* kmalloc_aligned(size_t size);

void* kcalloc(size_t num, size_t size);

void kfree(void* ptr);

uint32_t kheap_get_used(void);
uint32_t kheap_get_free(void);

#endif