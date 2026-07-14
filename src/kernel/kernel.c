/* kernel.c - BenmoshOS kernel entry point
 *
 * FIXES APPLIED:
 *  - Correct boot order: IDT -> memory -> heap -> print (malloc needed before print history)
 *  - STI moved BEFORE jump_usermode so keyboard IRQs work in ring3
 *  - Ring3 user program uses syscall 2 (print string) with a proper EBX string pointer
 *  - Welcome banner with color printed before ring3 drop
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
#include "../shell/shell.h"
#include "tss.h"

/* External: assembly trampoline that IRET-jumps into ring3 */
extern void jump_usermode(void* function_pointer);

/*
 * Ring3 user program.
 * Syscall ABI (int 0x80):
 *   EAX = syscall number
 *   EBX = arg1 (string pointer for syscall 2)
 *   ECX = arg2
 *   EDX = arg3
 *
 * Syscall numbers (see system.c syscall_dispatcher):
 *   1 = kernel test ping
 *   2 = print null-terminated string at EBX
 *   3 = yield (future use)
 */
void my_user_program() {
__asm__ volatile(
        ".intel_syntax noprefix\n"
        
        "loop_program:\n"
        // 1. Prompt the user for input
        "mov eax, 2\n"
        "mov ebx, offset prompt_msg\n"
        "int 0x80\n"
        
        // 2. Execute Syscall 4 to safely wait for keyboard input
        // Pass maximum string length limit of 64 bytes into ECX
        "mov eax, 4\n"
        "mov ebx, 64\n"
        "int 0x80\n"
        
        // Save the returned input buffer string memory address (EAX) onto the stack
        "push eax\n"
        
        // 3. Print the decorative prefix header layout
        "mov eax, 2\n"
        "mov ebx, offset echo_msg\n"
        "int 0x80\n"
        
        // 4. Pop the saved input string pointer back out into EBX and print it
        "pop ebx\n"
        "mov eax, 2\n"
        "int 0x80\n"
        
        // 5. Append a closing newline character for formatting cleanliness
        "mov eax, 2\n"
        "mov ebx, offset newline_msg\n"
        "int 0x80\n"
        
        "jmp loop_program\n"
        
        // String constants declared locally inside user execution space
        "prompt_msg:  .asciz \"Enter text: \"\n"
        "echo_msg:    .asciz \"[Echo back]: \"\n"
        "newline_msg: .asciz \"\\n\"\n"
        ::: "eax", "ebx", "ecx", "edx"
    );
    while (1) {  }
}

__attribute__((section(".text.main")))
void KernelMain() {
    /* CPU infrastructure */
    load_idt();
    pic_remap();

    /* Memory  */
    pmm_init();
    vmm_init();
    heap_init();

    /* Activete the TSS - hardware Task State Segment gate */
    init_tss();

    uint32_t user_stack_phys = pmm_alloc_block();

    vmm_map_page(user_stack_phys, 0x009FF000, 7);

    /* Print subsystem — enable_dynamic_history */
    enable_dynamic_history(1000);
    screen_clear();

    
    set_print_color(0x0A);
    print("  ____                                 _      ___  ____\n");
    print(" | __ )  ___ _ __  _ __ ___   ___  ___| |__  / _ \\/ ___|\n");
    print(" |  _ \\ / _ \\ '_ \\| '_ ` _ \\ / _ \\/ __| '_ \\| | | \\___ \\\n");
    print(" | |_) |  __/ | | | | | | | | (_) \\__ \\ | | | |_| |___) |\n");
    print(" |____/ \\___|_| |_|_| |_| |_|\\___/|___/_| |_|\\___/|____/\n");
    set_print_color(0x0B);
    print("\n  BenmoshOS v0.2  |  x86 bare-metal  |  Ring0 + Ring3\n");
    set_print_color(0x07);
    print("  Type 'help' for help\n\n");


    /* Enable hardware interrupts BEFORE ring3 jump
     *    (keyboard IRQ must fire while ring3 is spinning) */
    __asm__ volatile("sti");

    /* Print prompt then drop to ring3 */
    set_print_color(0x0E);
    print("BenmoshOS> ");
    set_print_color(0x0F);

    jump_usermode(my_user_program);

    /* Never reached — all further work done in IRQ handlers */
    while (1) { __asm__ volatile("hlt"); }
}