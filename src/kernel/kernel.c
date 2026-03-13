/* * kernel.c - The main kernel code for our OS
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

 // External assembly function to drop to Ring 3
extern void jump_usermode(void* function_pointer);

// Our very first Ring 3 user application!
void my_user_program() {
    // 1. Put the system call number (1) into the EAX register
    // 2. Trigger interrupt 0x80
    __asm__ volatile("mov $1, %eax; int $0x80");

    while (1) {
        // Trapped safely in User Mode
    }
}

__attribute__((section(".text.main")))
void main() {
    // 1. Hardware Interrupts
    load_idt();
    pic_remap();

    // 2. Memory Subsystems
    pmm_init();
    vmm_init();
    heap_init();

    // 3. User Interface
    enable_dynamic_history(1000);
    screen_clear();

    // 4. Security Sandbox (Ring 3 Preparation)
    init_tss();

    // 5. Enable hardware interrupts (Keyboard, Timer)
    jump_usermode(my_user_program);
    __asm__ volatile ("sti");

    print("Welcome to BenmoshOS!\n");
    print("All memory subsystems initialized.\n\n");

    print("Dropping to Ring 3 User Mode...\n");
    print("BenmoshOS> ");
    // Execute the jump!

    // The CPU should never reach this line because it is trapped in my_user_program
    while (1) {
        __asm__ volatile ("hlt");
    }
}