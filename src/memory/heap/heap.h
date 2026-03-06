#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

// Initialize the heap
void heap_init();

// Allocate memory
void* malloc(uint32_t size);

// Free memory
void free(void* ptr);

#endif