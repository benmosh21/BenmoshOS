#include "tss.h"

// Define the global instance declared in the header file
tss_entry_t g_tss;

// Reference the globally visible start marker of your GDT table inside boot.asm
extern uint32_t gdt_start; 

// Reference your assembly function wrapper that executes the 'ltr' machine instruction
extern void flush_tss(void);

void init_tss(void) {
    // Zero out the hardware tracking block entirely
    memset((unsigned char*)&g_tss, 0, sizeof(tss_entry_t));
    
    // Configure safe entry values for kernel space execution recovery loops
    g_tss.ss0 = 0x10;          // Targets your Kernel Data segment descriptor offset (index 2)
    g_tss.esp0 = 0x90000;      // Matches your safe execution stack top loaded in assembly
    
    // Disable the I/O permission bitmap checks by pointing its base beyond the structure limit
    g_tss.iomap_base = sizeof(tss_entry_t); 

    uint32_t base = (uint32_t)&g_tss;
    
    // set the gdt_tss
    gdt_set_gate(5, base, 0x67, 0x89, 0);

    // Safely update the Task Register (TR) via our assembly call instruction
    flush_tss();
}