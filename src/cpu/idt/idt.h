/*
 * idt.h - IDT definitions for our OS
 * This file contains the definitions for the Interrupt Descriptor Table (IDT) and related structures.
*/

#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include "../../drivers/print.h"

// IDT (Interrupt Descriptor Table) definitions
struct idt_entry {
    uint16_t offset_low; // lower 16 bits of handler function address
    uint16_t selector;   // code segment selector in GDT
    uint8_t zero;        // always zero
    uint8_t type_attr;   // type and attributes
    uint16_t offset_high; // higher 16 bits of handler function address
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit; // size of the IDT
    uint32_t base;  // base address of the IDT
} __attribute__((packed));

// Structure to hold the state of the CPU registers during an interrupt
struct interrupt_registers {
   uint32_t ds;                                     // Data segment selector
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pushad
   uint32_t int_no, err_code;                       // Pushed by us manually
   uint32_t eip, cs, eflags, useresp, ss;           // Pushed by the CPU automatically
};

// IDT (Interrupt Descriptor Table) definitions
extern struct idt_entry idt[256];
extern struct idt_ptr idt_ptr;


// Interrupt Service Routines (ISRs) declarations
void set_idt_gate(int num, uint32_t base, uint16_t sel, uint8_t flags);
void load_idt();

// ISR handler declaration
void isr_handler(struct interrupt_registers regs);

#endif