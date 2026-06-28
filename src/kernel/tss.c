#include "tss.h"
#include <string.h>

// Define the global instance declared in the header file
tss_entry_t g_tss;

// Reference the globally visible start marker of your GDT table inside boot.asm
extern uint32_t gdt_start; 

// Reference your assembly function wrapper that executes the 'ltr' machine instruction
extern void flush_tss(void);

void init_tss(void) {
    // Zero out the hardware tracking block entirely
    memset(&g_tss, 0, sizeof(tss_entry_t));
    
    // Configure safe entry values for kernel space execution recovery loops
    g_tss.ss0 = 0x10;          // Targets your Kernel Data segment descriptor offset (index 2)
    g_tss.esp0 = 0x90000;      // Matches your safe execution stack top loaded in assembly
    
    // Disable the I/O permission bitmap checks by pointing its base beyond the structure limit
    g_tss.iomap_base = sizeof(tss_entry_t); 

    uint32_t base = (uint32_t)&g_tss;
    
    // Mathematically locate descriptor entry index 5 (offset 40 bytes from gdt_start)
    // 5 preceding entries (Null, K-Code, K-Data, U-Code, U-Data) * 8 bytes each = 40 bytes
    uint8_t *gdt_tss_entry = (uint8_t *)&gdt_start + 40;

    // Slice and write the real runtime address bytes into split GDT base location fields
    gdt_tss_entry[2] = (base & 0xFF);         // Base bits 0-7
    gdt_tss_entry[3] = (base >> 8) & 0xFF;    // Base bits 8-15
    gdt_tss_entry[4] = (base >> 16) & 0xFF;   // Base bits 16-23
    gdt_tss_entry[7] = (base >> 24) & 0xFF;   // Base bits 24-31

    // Safely update the Task Register (TR) via our assembly call instruction
    flush_tss();
}