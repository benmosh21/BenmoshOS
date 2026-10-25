#include "gdt.h"

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // Get the location of gdt_start so i can set the bits in the location for the GDT entry
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) gdtr;

    __asm__ volatile ("sgdt %0" : "=m"(gdtr));
    uint8_t* gdt_entry = (uint8_t*)gdtr.base + (8 * num);

    gdt_entry[0] = (limit & 0xFF);                                  // Limit (7:0)
    gdt_entry[1] = (limit >> 8) & 0xFF;                             // Limit (15:8)
    gdt_entry[2] = (base & 0xFF);                                   // Base (7:0)
    gdt_entry[3] = (base >> 8) & 0xFF;                              // Base (15:8)
    gdt_entry[4] = (base >> 16) & 0xFF;                             // Base (23:16)
    gdt_entry[5] = access;                                          // Access Byte
    gdt_entry[6] = ((gran & 0x0F) << 4) | ((limit >> 16) & 0x0F);   // Flags (Fixed operator precedence)
    gdt_entry[7] = (base >> 24) & 0xFF;                             // Base (31:24)
}