#include "vmm.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));

void vmm_init() {
    // 1. Fill the first page table to map the first 4 MB of physical memory
    for (int i = 0; i < 1024; i++) {
        // Address = i * 4096, Flags = 3 (Present and Read/Write)
        first_page_table[i] = (i * 4096) | 3;
    }

    // 2. Put the page table into the very first slot of the page directory
    // Flags = 3 (Present and Read/Write)
    page_directory[0] = ((uint32_t)first_page_table) | 3;

    // 3. Load the physical address of the Page Directory into the CR3 register
    __asm__ volatile("mov %0, %%cr3" : : "r"(page_directory));

    // 4. Enable Paging by flipping Bit 31 of the CR0 register to 1	uint32_t cr0;
	__asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));

	cr0 |= 0x80000000; // Set the PG bit

	__asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}