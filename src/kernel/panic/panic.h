#ifndef PANIC_H
#define PANIC_H

// Makes a panic message and then hlt
void kernel_panic(const char* message, ...);

#endif