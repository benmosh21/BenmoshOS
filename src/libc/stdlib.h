#ifndef STDLIB_H
#define STDLIB_H

#include <stdint.h>

void reverse(char s[]);
void itoa(int n, char str[]);

/* Memory allocation exposed from heap.c */
void* malloc(uint32_t size);
void free(void* ptr);

#endif