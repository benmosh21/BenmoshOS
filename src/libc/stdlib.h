#ifndef STDLIB_H
#define STDLIB_H

#include <stdint.h>

void reverse(char s[]);
void itoa(int n, char str[]);
int atoi(const char* str);

void itoh(int n, char str[], int* start);

void sleep(uint32_t ms);

/* user space memory allocation */
void* malloc(uint32_t size);
void free(void* ptr);

#endif