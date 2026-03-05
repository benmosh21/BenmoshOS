#ifndef PMM_H
#define PMM_H

#include <stdint.h>

void pmm_init();

uint32_t pmm_alloc_block();

struct memorymap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext;
} __attribute__((packed));

#endif