#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Initialize the Virtual Memory Manager (VMM)
void vmm_init();

// Map a virtual address to a physical address with given flags
void vmm_map_page(uint32_t virtual_address, uint32_t physical_address, uint32_t flags);

#endif