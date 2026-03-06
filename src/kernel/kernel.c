/* 
 * kernel.c - The main kernel code for our OS
 * This file contains the entry point for the kernel and initializes the system.
 */

#include <stdint.h>
#include "system.h"
#include "../cpu/idt/idt.h"
#include "../cpu/pic/pic.h"
#include "../drivers/print/print.h"
#include "../drivers/ata/ata.h"
#include "../fs/fat16.h"
#include "../memory/pmm/pmm.h"
#include "../memory/vmm/vmm.h"
#include "../memory/heap/heap.h"


void test_memory() {
	print("--- STARTING MEMORY TEST ---\n");

    // ==========================================
    // TEST 1: PMM (Physical Memory Manager)
    // ==========================================
    uint32_t frame1 = pmm_alloc_block();
    uint32_t frame2 = pmm_alloc_block();

    print("PMM Frame 1: 0x"); print_int(frame1); print("\n");
    print("PMM Frame 2: 0x"); print_int(frame2); print("\n");
    // You should see that Frame 2 is exactly 0x1000 greater than Frame 1.
}

__attribute__((section(".text.main")))
void main() {
    load_idt(); // Load the Interrupt Descriptor Table (IDT)
    pic_remap(); // Remap PIC to avoid conflicts with CPU exceptions

	pmm_init(); // Initialize the Physical Memory Manager (PMM)

	vmm_init(); // Initialize the Virtual Memory Manager (VMM)

	heap_init(); // Initialize the heap for dynamic memory allocation

    __asm__ volatile ("sti"); // Enable interrupts

    print("Welcome to BenmoshOS!\n");
    print("This is a simple kernel written in C.\n");


	test_memory(); // Run the memory test to verify PMM and VMM functionality

    print("BenmoshOS> ");
    
    //int x = 1/0;

    while(1) {
        __asm__ volatile ("hlt"); // Halt the CPU until the next interrupt
    }
}