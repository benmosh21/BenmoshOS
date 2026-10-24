#include "vmm.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));
uint32_t heap_page_table[1024] __attribute__((aligned(4096)));

void vmm_init() {
    // Fill the first page table to map the first 4 MB of physical memory
    for (int i = 0; i < 1024; i++) {
        // Address = i * 4096, Flags = 3 (Present and Read/Write, Supervisor only)
        first_page_table[i] = (i * 4096) | 3;
    }

    // Map the next 4 MB for the Heap (0x00400000 - 0x007FFFFF)
    for (int i = 0; i < 1024; i++) {
        heap_page_table[i] = ((i + 1024) * 4096) | 3; // Offset physical addresses by 4MB
    }

    // Put the page tables into the page directory
    page_directory[0] = ((uint32_t)first_page_table) | 3;
    page_directory[1] = ((uint32_t)heap_page_table) | 3; // Link the heap page table

    // Load the physical address of the Page Directory into CR3
    __asm__ volatile("{mov %0, %%cr3 | mov cr3, %0}" : : "r"(page_directory));

    // Enable Paging
    uint32_t cr0;
    __asm__ volatile ("{mov %%cr0, %0 | mov %0, cr0}" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("{mov %0, %%cr0 | mov cr0, %0}" : : "r"(cr0));
}

void vmm_map_page(uint32_t physical_address, uint32_t virtual_address, uint32_t flags) {

    if (virtual_address & 0xFFF) {
        kernel_panic("vmm_map_page: Virtual address 0x%x is not 4KB aligned!", virtual_address);
        return;
    }

    if (pmm_is_frame_free(physical_address >> 12)) {
        kernel_panic("vmm_map_page: Physical address 0x%x is not allocated in PMM!", physical_address);
        return;
    }

    uint32_t irq_state = irq_save();

    // Calculate the indices for the Page Directory and Page Table
    uint32_t pdi = virtual_address >> 22;
    uint32_t pti = (virtual_address >> 12) & 0x03FF;

    // Check if the Page Table exists in the Directory
    if ((page_directory[pdi] & 1) == 0) {

        // The table does not exist. Ask the PMM for a raw 4KB hardware frame.
        uint32_t new_table_physical = pmm_alloc_frame();
        if (!new_table_physical) kernel_panic("VMM Out of Memory! PMM returned 0.");

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

    irq_restore(irq_state);
}

void vmm_unmap_page(uint32_t virtual_address) {

    uint32_t irq_state = irq_save();

    uint32_t pdi = virtual_address >> 22;
    uint32_t pti = (virtual_address >> 12) & 0x03FF;

    if ((page_directory[pdi] & 1) == 0) {
        // The page table does not exist; nothing to unmap
        return ;
    }

    // Temporarily map the target page table to the scratchpad virtual address
    extern uint32_t first_page_table[];
    first_page_table[1023] = (page_directory[pdi] & ~0xFFF) | 3;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");
    
    // Access the page table via the virtual scratchpad address
    uint32_t* page_table = (uint32_t*)0x003FF000;
    page_table[pti] = 0; // Clear the entry to unmap

    // Unmap the scratchpad
    first_page_table[1023] = 0;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    // Flush the TLB for the requested virtual address
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (virtual_address) : "memory");

    irq_restore(irq_state);
}

uint32_t vmm_get_physical(uint32_t virtual_address) {

    uint32_t irq_state = irq_save();

    uint32_t pdi = virtual_address >> 22;
    uint32_t pti = (virtual_address >> 12) & 0x03FF;
    uint32_t offset = virtual_address & 0xFFF;

    if ((page_directory[pdi] & 1) == 0) {
        // The page table does not exist; return 0 to indicate no mapping
        return 0;
    }

    // Temporarily map the target page table to the scratchpad virtual address
    extern uint32_t first_page_table[];
    first_page_table[1023] = (page_directory[pdi] & ~0xFFF) | 3;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    // Access the page table via the virtual scratchpad address
    uint32_t* page_table = (uint32_t*)0x003FF000;
    
    uint32_t pte = page_table[pti];
    uint32_t physical = 0;

    // Check if the page is present and retrieve the physical address
    if (pte & 1) {
        physical = (pte & ~0xFFF) | offset; // Mask out the flags to get the physical address
    }

    // Unmap the scratchpad
    first_page_table[1023] = 0;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    irq_restore(irq_state);

    return physical;
}


/**
 * @brief Destroy an address space and frees its physical frames.
 * 
 * Walks user-space page directory entries (PDE 2-1023), frees all
 * underlying page tables frames, and releases the page directory frame.
 * 
 * @param page_directory_phys Physical address of the page directory to destroy.
 */
void vmm_destroy_address_space(uint32_t* page_directory_phys)
{
    if (!page_directory_phys) {
        kernel_panic("vmm_destroy_address_space: NULL page directory pointer!");
        return;
    }

    uint32_t flags = irq_save();

    extern uint32_t first_page_table[];

    // Map target page directory to scatchpad
    first_page_table[1023] = ((uint32_t)page_directory_phys) | 3;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    uint32_t* pdir = (uint32_t*)0x003FF000;

    for (int i = 2; i < 1024; i++) {
        if (pdir[i] & 1) {
            uint32_t page_table_phys = pdir[i] & ~0xFFF;

            // Free the page table frame
            pmm_free_block(page_table_phys, 1);
        }
    }

    // Unmap the scratchpad
    first_page_table[1023] = 0;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    // Free the page directory frame
    pmm_free_block((uint32_t)page_directory_phys, 1);

    irq_restore(flags);
}


/*
 * @brief Create a new independent virtual address space for a process.
 *
 * Allocates a 4KB physical frame of the page directory.
 * Clones the kernel mappings (PED 0 and 1) into the new page directory with Supervisor-only
 * permissions (flag = 3) so kernel space is shared and protected.
 * Clears all user space entries (PDE 2-1023) to ensure a clean slate for the new process.
 * 
 * @return Physical address of the newly created page directory (loaded into CR3).
*/
uint32_t *vmm_create_address_space()
{
    uint32_t pdir_phys = pmm_alloc_frame();
    if (!pdir_phys) {
        printf("[DEBUG] pmm_alloc_frame returned 0 (OOM or Frame 0)\n");
        return NULL; // Out of memory
    }

    uint32_t flags = irq_save();

    /*
     * we cannot write to the physical address directly, while paging is enabled.
     * We map the last page of table 0 (virtual 0x003FF000) to this farme.
    */

    extern uint32_t first_page_table[];
    first_page_table[1023] = pdir_phys | 3; // Map the new page directory to the scratchpad
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    uint32_t *new_pdir = (uint32_t *)0x003FF000;

    /* 
     * Clone the kernel mapping
     * PDE 0 (0 - 4MB): Kernel code, data, hardware mappings
     * PDE 1 (4MB - 8MB): Kernel heap
     * Use flags = 3 (Present + Read/Write, Supervisor only) to ensure kernel space is protected.    
    */
    new_pdir[0] = ((uint32_t)first_page_table[0]) | 3; // Clone PDE 0
    extern uint32_t heap_page_table[];
    new_pdir[1] = ((uint32_t)heap_page_table[0]) | 3; // Clone PDE 1

    /*
     * Clear all user space mapping (PDE 2 -1023)
     * These will be populated on demand by user ELF segments and stack.
    */

    for (int i = 2; i < 1024; i++) {
        new_pdir[i] = 0; // Clear user space PDEs
    }

    /* Clone Kernel Mappings directly from the master page_directory */
    extern uint32_t page_directory[];
    new_pdir[0] = page_directory[0] & ~4; /* Ensure supervisor bit (clear user bit 2) */
    new_pdir[1] = page_directory[1] & ~4;

    // Unmap the scratchpad
    first_page_table[1023] = 0;
    __asm__ volatile("{invlpg (%0) | invlpg [%0]}" ::"r" (0x003FF000) : "memory");

    irq_restore(flags);
    return (uint32_t *)pdir_phys; // Return the physical address of the new
}
