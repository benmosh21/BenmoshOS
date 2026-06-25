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
        "mov $2,  %%eax\n"
        "lea msg, %%ebx\n"
        "int $0x80\n"
        "jmp done\n"
        "msg: .asciz \"[Ring3] Hello from user space!\\n\"\n"
        "done:\n"
        ::: "eax", "ebx"
    );
    while (1) { __asm__ volatile("hlt"); }
}

__attribute__((section(".text.main")))
void main() {
    /* 1. CPU infrastructure */
    load_idt();
    pic_remap();

    /* 2. Memory (MUST come before any malloc/print-history calls) */
    pmm_init();
    vmm_init();
    heap_init();

    /* 3. Print subsystem — enable_dynamic_history calls malloc */
    enable_dynamic_history(1000);
    screen_clear();

    /* 4. Welcome banner */
    set_print_color(0x0A);
    print("  ____                                 _      ___  ____\n");
    print(" | __ )  ___ _ __  _ __ ___   ___  ___| |__  / _ \\/ ___|\n");
    print(" |  _ \\ / _ \\ '_ \\| '_ ` _ \\ / _ \\/ __| '_ \\| | | \\___ \\\n");
    print(" | |_) |  __/ | | | | | | | | (_) \\__ \\ | | | |_| |___) |\n");
    print(" |____/ \\___|_| |_|_| |_| |_|\\___/|___/_| |_|\\___/|____/\n");
    set_print_color(0x0B);
    print("\n  BenmoshOS v0.2  |  x86 bare-metal  |  Ring0 + Ring3\n");
    set_print_color(0x07);
    print("  Type 'stupid' for help\n\n");

    /* 5. TSS — must be done before ring3 jump */
    init_tss();

    /* 6. Enable hardware interrupts BEFORE ring3 jump
     *    (keyboard IRQ must fire while ring3 is spinning) */
    __asm__ volatile("sti");

    /* 7. Print prompt then drop to ring3 */
    set_print_color(0x0E);
    print("BenmoshOS> ");
    set_print_color(0x0F);

    //jump_usermode(my_user_program);

    /* Never reached — all further work done in IRQ handlers */
    while (1) { __asm__ volatile("hlt"); }
}