#include "vmm.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));
uint32_t heap_page_table[1024] __attribute__((aligned(4096)));

void vmm_init() {
    // Fill the first page table to map the first 4 MB of physical memory
    for (int i = 0; i < 1024; i++) {
        // Address = i * 4096, Flags = 3 (Present and Read/Write)
        first_page_table[i] = (i * 4096) | 7;
    }

    // Map the next 4 MB for the Heap (0x00400000 - 0x007FFFFF)
    for (int i = 0; i < 1024; i++) {
        heap_page_table[i] = ((i + 1024) * 4096) | 3; // Offset physical addresses by 4MB
    }

    // Put the page tables into the page directory
    page_directory[0] = ((uint32_t)first_page_table) | 7;
    page_directory[1] = ((uint32_t)heap_page_table) | 3; // Link the heap page table

    // Load the physical address of the Page Directory into CR3
    __asm__ volatile("{mov %0, %%cr3 | mov cr3, %0}" : : "r"(page_directory));

    // Enable Paging
    uint32_t cr0; // Fixed: Moved out of the comment
    __asm__ volatile ("{mov %%cr0, %0 | mov %0, cr0}" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("{mov %0, %%cr0 | mov cr0, %0}" : : "r"(cr0));
}

void vmm_map_page(uint32_t physical_address, uint32_t virtual_address, uint32_t flags) {

    if (virtual_address & 0xFFF) {
        puts("[VMM] Error: Virtual address is not page-aligned.\n");
        return;
    }

    if (pmm_is_frame_free(physical_address >> 12)) {
        puts("[VMM] Error: Physical address is not allocated.\n");
        return;
    }

    // Calculate the indices for the Page Directory and Page Table
    uint32_t pdi = virtual_address >> 22;
    uint32_t pti = (virtual_address >> 12) & 0x03FF;

    // Check if the Page Table exists in the Directory
    if ((page_directory[pdi] & 1) == 0) {

        // The table does not exist. Ask the PMM for a raw 4KB hardware frame.
        uint32_t new_table_physical = pmm_alloc_block();

        // Link the physical frame into the directory (Set Present + Read/Write)
        page_directory[pdi] = new_table_physical | 7;

        // --- THE SCRATCHPAD ROUTINE ---
        // We must zero out the new table, but we cannot write to new_table_physical directly.
        // We temporarily map our virtual "scratchpad" (the last page of the first 4MB) to this frame.
        extern uint32_t first_page_table[];
        first_page_table[1023] = new_table_physical | 7;

        // Flush the TLB to force the CPU to recognize the temporary mapping
        __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

        // Safely zero the memory using the virtual pointer
        uint32_t* scratchpad = (uint32_t*)0x003FF000;
        for (int i = 0; i < 1024; i++) {
            scratchpad[i] = 0;
        }

        // Unmap the scratchpad to prevent accidental writes later
        first_page_table[1023] = 0;
        __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");
    }

    // Temporarily map the target page table to the scratchpad virtual address
    extern uint32_t first_page_table[];
    first_page_table[1023] = (page_directory[pdi] & ~0xFFF) | 3;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    // Access the page table via the virtual scratchpad address
    uint32_t* page_table = (uint32_t*)0x003FF000;
    page_table[pti] = physical_address | flags;

    // Unmap the scratchpad
    first_page_table[1023] = 0;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    // Flush the TLB for the requested virtual address
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (virtual_address) : "memory");
}

vmm_unmap_page(uint32_t virtual_address) {
    uint32_t pdi = virtual_address >> 22;
    uint32_t pti = (virtual_address >> 12) & 0x03FF;
    uint32_t offset = virtual_address & 0xFFF;

    if ((page_directory[pdi] & 1) == 0) {
        // The page table does not exist; nothing to unmap
        return;
    }

    
}