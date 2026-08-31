#include "strings.h"

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

int strcmp(char* s1, char* s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return 1;
}

int memcmp(const char* s1, const char* s2, int count) {
    for (int i = 0; i < count; i++) {
        if (s1[i] != s2[i]) {
            return 1;
        }
    }
    return 0;
}