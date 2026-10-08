#ifndef STRING_H
#define STRING_H

#include <stdint.h>

unsigned char* memcpy(unsigned char* dest, const uint8_t* src, uint32_t count);
unsigned char* memset(unsigned char* dest, uint8_t val, uint32_t count);
unsigned short* memsetw(unsigned short* dest, uint16_t val, uint32_t count);
int strlen(const char* str);
int strcmp(char* s1, char* s2);
int memcmp(const char* s1, const char* s2, int count);

#endif