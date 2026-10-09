#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

/* ---- I/O port ---- */

// Read byte from port
static inline uint8_t inportb(uint16_t _port) {
    unsigned char rv;
    __asm__ volatile ("{inb %1, %0 | in %0, %1}" : "=a" (rv) : "Nd" (_port));
    return rv;
}

// Read word from port
static inline uint16_t inportw(uint16_t _port) {
    uint16_t rv;
    __asm__ volatile ("{inw %1, %0 | in %0, %1}" : "=a" (rv) : "Nd" (_port));
    return rv;
}

// Write byte from port
static inline void outportb(uint16_t _port, unsigned char _data) {
    __asm__ volatile ("{outb %0, %1 | out %1, %0}" : : "a" (_data), "Nd" (_port));
}

// Read word from port
static inline void outportw(uint16_t _port, uint16_t _data) {
    __asm__ volatile ("{outw %0, %1 | out %1, %0}" : : "a" (_data), "Nd" (_port));
}

// Wait for I/O operation to complete
static inline void io_wait(void) {
    outportb(0x80, 0);
}

#endif