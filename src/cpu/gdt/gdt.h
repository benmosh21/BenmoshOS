#ifndef GDT_H
#define GDT_H

#include <stdint.h>

extern uint32_t gdt_start;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

#endif