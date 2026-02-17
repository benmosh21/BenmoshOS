/* 
 * kernel.c - The main kernel code for our OS
 * This file contains the entry point for the kernel and initializes the system.
 */

#include <stdint.h>
#include "system.h"
#include "../cpu/idt/idt.h"
#include "../drivers/print.h"


__attribute__((section(".text.main")))
void main() {
    load_idt();

    char *screen = (char*) 0xb8000;
    char *str = "this is from C";
    
    screen += 160;
    print(str, screen);
    
    screen += 160;
    print(str, screen);

    int x = 1/0; // This will trigger a divide by zero exception

    while(1);
}
