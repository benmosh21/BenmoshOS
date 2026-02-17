/*
 * system.h - System definitions for our OS
 * This file contains definitions and declarations for system-level functions.
 */
#ifndef __SYSTEM_H
#define __SYSTEM_H

// Standard types
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;

/* MAIN.C */
extern unsigned char *memcpy(unsigned char *dest, const uint8_t *src, uint32_t count);
extern unsigned char *memset(unsigned char *dest, uint8_t val, uint32_t count);
extern unsigned short *memsetw(unsigned short *dest, uint16_t val, uint32_t count);
extern int strlen(const char *str);
extern unsigned char inportb (uint16_t _port);
extern void outportb (uint16_t _port, unsigned char _data);

extern void cls();
extern void putch(unsigned char c);
extern void puts(unsigned char *str);
extern void settextcolor(unsigned char forecolor, unsigned char backcolor);
extern void init_video();

#endif