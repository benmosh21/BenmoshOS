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


