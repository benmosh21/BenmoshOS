#ifndef PMM_H
#define PMM_H

#include <stdint.h>


// Initialize the Physical Memory Manager (PMM)
void pmm_init();

// Allocate a single 4KB block of physical memory and return its physical address
uint32_t pmm_alloc_block();

// Free a previously allocated block of physical memory
void pmm_free_block(uint32_t physical_address, uint32_t num_frames);

// Reserve a block of physical memory (mark it as allocated without returning it)
void pmm_reserve_block(uint32_t physical_address, uint32_t size);

struct memorymap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext;
} __attribute__((packed));

#endif