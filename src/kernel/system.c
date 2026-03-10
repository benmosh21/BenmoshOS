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


void init_tss() {
	gdt_start[40 + 5] = 0xe9; // Access byte: present, ring 0, type 9 (32-bit TSS)
	gdt_start[40 + 6] = 0x00; // Limit low byte

	for (int i = 0; i < sizeof(tss_entry); i++) {
		((uint8_t*)&tss_entry)[i] = 0; // Clear the TSS entry
	}

	tss_entry.ss0 = 0x10; // Ring 0 data segment selector
	tss_entry.esp0 = 0x90000; // Stack pointer for Ring 0 (top of our 4KB stack)
	tss_entry.iomap_base = sizeof(tss_entry_t); // No I/O bitmap, so set to end of TSS

    flush_tss();
}
