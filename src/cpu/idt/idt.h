/*
 * idt.h - IDT definitions for our OS
 * This file contains the definitions for the Interrupt Descriptor Table (IDT) and related structures.
*/

#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include "../../drivers/print/print.h"
#include "../../drivers/keyboard/keyboard.h"
#include "../pic/pic.h" // For sending EOI to PICs after handling interrupts


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

// ISR (Interrupt Service Routine) declarations
extern void isr0(); 
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void isr32();
extern void isr33();
extern void isr34();
extern void isr35();
extern void isr36();
extern void isr37();
extern void isr38();
extern void isr39();
extern void isr40();
extern void isr41();
extern void isr42();
extern void isr43();
extern void isr44();
extern void isr45();
extern void isr46();
extern void isr47();

extern void isr128(); // System call interrupt (int 0x80)

// Interrupt Service Routines (ISRs) declarations
void set_idt_gate(int num, uint32_t base, uint16_t sel, uint8_t flags);
void load_idt();

// ISR handler declaration
void isr_handler(struct interrupt_registers regs);

#endif