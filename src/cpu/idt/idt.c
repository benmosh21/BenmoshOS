/*
 * idt.c - IDT functions for our OS
 * This file contains functions to set up and load the Interrupt Descriptor Table (IDT).
 */

#include "idt.h"

// Define the IDT and its pointer
struct idt_entry idt[256];
struct idt_ptr idt_ptr;

// IDT functions
void set_idt_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

void load_idt() {
    idt_ptr.limit = sizeof(struct idt_entry) * 256 - 1;
    idt_ptr.base = (uint32_t) &idt;

    extern void isr0(); // Declare ISR handlers (defined in interrupts.asm)
    set_idt_gate(0, (uint32_t) isr0, 0x08, 0x8E); // Set the first entry for divide by zero exception

    __asm__ volatile ("lidt (%0)" : : "r" (&idt_ptr));
}

void isr_handler(struct interrupt_registers regs) {
    switch (regs.int_no) {
        case 0x00:
            print("Divide by zero exception", (char*) 0xb8000);
            for (;;);
            break;
        case 0x01:
            // Handle debug exception
            break;
        default:
            // Handle unknown interrupt
            break;
    }
    print("Unhandled interrupt: ", (char*) 0xb8200);
}