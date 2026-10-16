#include "panic.h"
#include "../../drivers/print/print.h"
#include "../../klib/stdio.h"
#include <stdarg.h>

// Makes a panic message and then hlt
void kernel_panic(const char* message, ...) {
    // disable interrupts
    __asm__ volatile("cli");

    // Print the panic message
    set_print_color(0x4F);      // White on red
    puts("\n\n[BENMOSH PANIC] ");

    va_list args;
    va_start(args, message);
    vprintf(message, args);
    va_end(args);

    puts("\n-- System Halted --\n");

    // Halt forever
    for (;;) __asm__ volatile("hlt");
}
