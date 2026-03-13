/*
 * system.c - System functions for our OS
 * This file contains utility functions that are used throughout the kernel.
 */

#include "system.h"

// Memory manipulation functions
// Copies 'count' bytes from 'src' to 'dest'
unsigned char *memcpy(unsigned char *dest, const uint8_t *src, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        dest[i] = src[i];
    }
    return dest;
}

// Sets 'count' bytes in 'dest' to the value 'val'
unsigned char *memset(unsigned char *dest, uint8_t val, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        dest[i] = val;
    }
    return dest;
}

// Sets 'count' 16-bit words in 'dest' to the value 'val'
unsigned short *memsetw(unsigned short *dest, uint16_t val, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        dest[i] = val;
    }
    return dest;
}

// Returns the length of a null-terminated string
int strlen(const char *str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int strcmp(char* s1, char* s2) {
    int i = 0;

    // Keep looping as long as the letters match
    while (s1[i] == s2[i]) {
        // If we reach the null terminator without any mismatches, it is a perfect match
        if (s1[i] == '\0') {
            return 1;
        }
        i++;
    }

    // If we exit the loop early, a letter didn't match
    return 0;
}

// I/O port functions
// Reads a byte from the specified I/O port
uint8_t inportb(uint16_t _port) {
    unsigned char rv;
    __asm__ volatile ("inb %1, %0" : "=a" (rv) : "Nd" (_port));
    return rv;
}

uint16_t inportw (uint16_t _port) {
    uint16_t rv;
    __asm__ volatile ("inw %1, %0" : "=a" (rv) : "Nd" (_port));
    return rv;
}

// Writes a byte to the specified I/O port
void outportb(uint16_t _port, unsigned char _data) {
    __asm__ volatile ("outb %0, %1" : : "a" (_data), "Nd" (_port));
}

void outportw(uint16_t _port, uint16_t _data) {
	__asm__ volatile ("outw %0, %1" : : "a" (_data), "Nd" (_port));
}


// Define the actual physical variable here in the C file
tss_entry_t tss_entry;

// Define the function body here
void flush_tss() {
    __asm__ volatile ("ltr %%ax" : : "a" (0x28));
}

// A tiny struct to hold the hardware's GDT pointer
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));


void init_tss() {
    // 1. ASK THE HARDWARE WHERE THE GDT IS
    struct gdt_ptr current_gdt;
    __asm__ volatile ("sgdt %0" : "=m" (current_gdt));

    // Create a pointer to the GDT in memory
    uint8_t* gdt_start = (uint8_t*)current_gdt.base;

    // 2. Get the physical memory address of the TSS struct, and its size
    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry_t);

    // 3. Splice the LIMIT into the GDT
    gdt_start[40 + 0] = limit & 0xFF;         // Limit lowest 8 bits
    gdt_start[40 + 1] = (limit >> 8) & 0xFF;  // Limit highest 8 bits

    // 4. Splice the BASE (memory address) into the GDT using bitwise shifts
    gdt_start[40 + 2] = base & 0xFF;          // Base lowest 8 bits
    gdt_start[40 + 3] = (base >> 8) & 0xFF;   // Base middle bits (8-15)
    gdt_start[40 + 4] = (base >> 16) & 0xFF;  // Base middle bits (16-23)
    gdt_start[40 + 7] = (base >> 24) & 0xFF;  // Base highest 8 bits (24-31)

    // 5. Set the Access and Flags
    gdt_start[40 + 5] = 0xE9; // Access byte: present, ring 3 allowed, 32-bit TSS
    gdt_start[40 + 6] = 0x00; // Flags

    // 6. Initialize the actual TSS Struct
    for (int i = 0; i < sizeof(tss_entry); i++) {
        ((uint8_t*)&tss_entry)[i] = 0;
    }

    tss_entry.ss0 = 0x10;
    tss_entry.esp0 = 0x90000;
    tss_entry.iomap_base = sizeof(tss_entry_t);

    // 7. Lock it into the CPU silicon
    flush_tss();
}

void syscall_dispatcher(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    if (eax == 1) {
        print("Kernel: Syscall 1 (Print) triggered from Ring 3!\n");
    }
    else {
        print("Kernel: Unknown syscall triggered.\n");
    }
}