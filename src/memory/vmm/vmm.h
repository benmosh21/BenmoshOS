#ifndef VMM_H
#define VMM_H

#include <stdint.h> 
#include <stddef.h> 
#include "../pmm/pmm.h"
#include "../../klib/stdio.h"
#include "../../kernel/panic/panic.h"

// Initialize the Virtual Memory Manager (VMM)
void vmm_init();

// Map a virtual address to a physical address with given flags
void vmm_map_page(uint32_t physical_address, uint32_t  virtual_address, uint32_t flags);

// Unmap a virtual address, effectively removing its mapping
void vmm_unmap_page(uint32_t virtual_address);

// Retrieve the physical address corresponding to a given virtual address
uint32_t vmm_get_physical(uint32_t virtual_address);

// Create a new virtual address space
uint32_t* vmm_create_address_space();
// Destroy a virtual address space
void vmm_destroy_address_space(uint32_t* page_directory_phys);

#endif