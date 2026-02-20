/* 
 * kernel.c - The main kernel code for our OS
 * This file contains the entry point for the kernel and initializes the system.
 */

#include <stdint.h>
#include "system.h"
#include "../cpu/idt/idt.h"
#include "../cpu/pic/pic.h"
#include "../drivers/print/print.h"
#include "../drivers/ata.h"


__attribute__((section(".text.main")))
void main() {
    load_idt(); // Load the Interrupt Descriptor Table (IDT)
    pic_remap(); // Remap PIC to avoid conflicts with CPU exceptions

    uint8_t boot_sector[512];
    ata_read_sector(0, boot_sector);

    if (boot_sector[510] == 0x55 && boot_sector[511] == 0xAA) {
        print("ATA Driver Working! Signature found.");
    } else {
        print("ATA Driver Falied.");
    }

    __asm__ volatile ("sti"); // Enable interrupts

    print("Welcome to BenmoshOS!\n");
    print("This is a simple kernel written in C.\n");
    

    while(1) {
        __asm__ volatile ("hlt"); // Halt the CPU until the next interrupt
    }
}