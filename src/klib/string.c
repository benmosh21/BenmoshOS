#include "string.h"

unsigned char* memcpy(unsigned char* dest, const uint8_t* src, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) dest[i] = src[i];
    return dest;
}

unsigned char* memset(unsigned char* dest, uint8_t val, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) dest[i] = val;
    return dest;
}

unsigned short* memsetw(unsigned short* dest, uint16_t val, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) dest[i] = val;
    return dest;
}

int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int memcmp(const char* s1, const char* s2, int count) {
    for (int i = 0; i < count; i++) {
        if (s1[i] != s2[i]) {
            return 1;
        }
    }
    return 0;
}

char *strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0'; // Null-terminate the destination string
    return original_dest;
}

char *strncpy(char* dest, const char* src, int n) {
    char* original_dest = dest;
    int i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0'; // Null-terminate the remaining space
    }
    return original_dest;
}