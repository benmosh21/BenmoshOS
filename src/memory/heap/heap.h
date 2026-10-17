#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>
#include "../cpu/ports/ports.h"

// define all important data
#define HEAP_START 0x400000
#define HEAP_SIZE 0x400000
#define HEAP_END 0x800000

// 8-byte metadata header
struct block_header {
    uint32_t size_and_free; // Top 31 bits = size, Bottom 1 bit = free flag
    struct block_header* next;          // Memory address of the next header
} __attribute__((packed));

extern struct block_header* heap_start;

// Initialize the heap
void heap_init(void);

// Allocate memory
void* kmalloc(uint32_t size);

// Free memory
void kfree(void* ptr);

void* kmalloc_aligned(uint32_t size, uint32_t alignment);
void kfree_aligned(void* ptr);

#endif