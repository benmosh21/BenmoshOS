/*
 * idt.c - IDT functions for our OS
 * This file contains functions to set up and load the Interrupt Descriptor Table (IDT).
 */

#include "idt.h"


#define SCANCODE_UP_ARROW   72
#define SCANCODE_DOWN_ARROW 80

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


// ISR stub table (defined in interrupts.asm)
void *isr_stub_table[48] = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
};

// Load the IDT into the CPU
void load_idt() {
    idt_ptr.limit = sizeof(struct idt_entry) * 256 - 1;
    idt_ptr.base = (uint32_t) &idt;

    for (int i = 0; i < 48; i++) {
        set_idt_gate(i, (uint32_t) isr_stub_table[i], 0x08, 0x8E);
    }

    __asm__ volatile ("lidt (%0)" : : "r" (&idt_ptr));
}

// ISR handler function
void isr_handler(struct interrupt_registers regs) {
    switch (regs.int_no) {
        case 0x00:
            print("Divide by zero exception\n");
            for (;;) {
                __asm__ volatile("hlt");
            }
            break;

        case 0x01:
            print("Debug exception\n");
            for (;;) __asm__ volatile("hlt");
            break;

        case 0xe:
            uint32_t faulting_address;
            __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_address));
			print("Page fault at address: 0x");
			print_hex(faulting_address);
			print("\n");
			for (;;) __asm__ volatile("hlt");
			break;
        
        case 0x20:
            // Timer interrupt (IRQ0)
            // We can add timer handling code here if needed
            print("test");
            break;

        case 0x21: 
            // Keyboard interrupt (IRQ 1)
            //print_scancode(inportb(0x60)); // Print the scancode for debugging
            keyboard_handler(); // Let the driver handle EVERYTHING
            break;

        case 0x2c:
            // Mouse interrupt (IRQ12)
            print("Mouse interrupt received\n");
            break;

        default:
            // Handle unknown interrupt
            print("Unhandled interrupt:\n");
            print_int(regs.int_no); // Let's see the number!
            print("\n");
            break;
        }

    if (regs.int_no >= 32 && regs.int_no < 48) {
        pic_send_eoi(regs.int_no - 32);
    }
}