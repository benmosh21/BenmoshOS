#ifndef TSS_H
#define TSS_H

#include <stdint.h>

// The 32-bit hardware Task State Segment structure matching x86 specifications
struct tss_entry_struct {
    uint32_t link;
    uint32_t esp0;     // Safe Kernel Stack Pointer loaded during Ring 3 -> Ring 0 transitions
    uint32_t ss0;      // Kernel Data Segment selector offset (0x10)
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

// Global instance of the hardware TSS tracking block
extern tss_entry_t g_tss;

/**
 * @brief Initializes the TSS structure fields, registers a secure landing stack,
 * and dynamically patches its base runtime address into the GDT layout.
 */
void init_tss(void);

#endif // TSS_H