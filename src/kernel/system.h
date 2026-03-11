/*
 * system.h - System definitions for our OS
 * This file contains definitions and declarations for system-level functions.
 */
#ifndef __SYSTEM_H
#define __SYSTEM_H

#include <stdint.h>

/* MAIN.C */
extern unsigned char *memcpy(unsigned char *dest, const uint8_t *src, uint32_t count);
extern unsigned char *memset(unsigned char *dest, uint8_t val, uint32_t count);
extern unsigned short *memsetw(unsigned short *dest, uint16_t val, uint32_t count);
int strlen(const char *str);
uint8_t inportb (uint16_t _port);
uint16_t inportw (uint16_t _port);
void outportb (uint16_t _port, unsigned char _data);
void outportw (uint16_t _port, uint16_t _data);
int strcmp(char* s1, char* s2);


// A structure that perfectly aligns with the x86 hardware TSS
struct tss_entry_struct {
    uint32_t prev_tss; // Hardware task linkage (Unused)
    uint32_t esp0;     // The Ring 0 Stack Pointer we care about!
    uint32_t ss0;      // The Ring 0 Stack Segment we care about!
    uint32_t esp1;     // Unused
    uint32_t ss1;      // Unused
    uint32_t esp2;     // Unused
    uint32_t ss2;      // Unused
    uint32_t cr3;      // Unused
    uint32_t eip;      // Unused
    uint32_t eflags;   // Unused
    uint32_t eax;      // Unused
    uint32_t ecx;      // Unused
    uint32_t edx;      // Unused
    uint32_t ebx;      // Unused
    uint32_t esp;      // Unused
    uint32_t ebp;      // Unused
    uint32_t esi;      // Unused
    uint32_t edi;      // Unused
    uint32_t es;       // Unused
    uint32_t cs;       // Unused
    uint32_t ss;       // Unused
    uint32_t ds;       // Unused
    uint32_t fs;       // Unused
    uint32_t gs;       // Unused
    uint32_t ldt;      // Unused
    uint16_t trap;     // Unused
    uint16_t iomap_base; // Unused
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

extern uint8_t gdt_start[];

// We use a global variable so the TSS stays in memory forever
extern tss_entry_t tss_entry;

void flush_tss();

void init_tss();

#endif