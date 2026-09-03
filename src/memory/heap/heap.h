#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

// define all important data
#define HEAP_START 0x400000
#define HEAP_SIZE 0x400000
#define HEAP_END 0x800000


// Initialize the heap
void heap_init();

// Allocate memory
void* malloc(uint32_t size);

// Free memory
void free(void* ptr);

#endif