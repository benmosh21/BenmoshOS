#ifndef PMM_H
#define PMM_H

#include <stdint.h>

struct memorymap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_ext;
} __attribute__((packed));

int pmm_is_frame_free(uint32_t frame);
void pmm_init();
int pmm_is_frame_free(uint32_t frame);
uint32_t pmm_alloc_block();
void pmm_free_block(uint32_t physical_address, uint32_t num_frames);
void pmm_reserve_block(uint32_t physical_address, uint32_t size);

void pmm_get_stats(uint32_t* out_total, uint32_t* out_free, uint32_t* out_used);


#endif