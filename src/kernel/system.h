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
extern int strlen(const char *str);
uint8_t inportb (uint16_t _port);
uint16_t inportw (uint16_t _port);
void outportb (uint16_t _port, unsigned char _data);
void outportw (uint16_t _port, uint16_t _data);
int strcmp(char* s1, char* s2);

#endif