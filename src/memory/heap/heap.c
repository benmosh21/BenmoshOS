#include "heap.h"
#include <stddef.h> // For NULL

// 8-byte metadata header
struct block_header {
    uint32_t size_and_free; // Top 31 bits = size, Bottom 1 bit = free flag
    uint32_t next;          // Memory address of the next header
} __attribute__((packed));

// We pre-mapped 4MB to 8MB in the VMM. 4MB in hex is 0x400000.
// This pointer will always remember where the heap starts.
struct block_header* first_header = (struct block_header*)0x400000;

void init_heap() {
    // We will write this soon to format the empty 4MB space!
}

void* malloc(uint32_t requested_size) {
// Align requested_size up to the nearest multiple of 4
    // This ensures the bottom bits (including our flag bit) are always 0
    if (requested_size % 4 != 0) {
        requested_size += 4 - (requested_size % 4);
    }

    struct block_header* current = first_header;

    while (current != NULL) {
        // Calculate current_size by masking out the flag bit
        uint32_t current_size = current->size_and_free & ~1;

        if ((current->size_and_free & 1) && (current_size >= requested_size)) {
            
            // Step A: Mark the block as USED
            current->size_and_free &= ~1;

            // Step B: Carve the block if it's bigger than we need
            if ((current_size - requested_size) >= 24) {
                struct block_header* new_block = (struct block_header*)((uint32_t)current + 8 + requested_size);
                
                new_block->size_and_free = (current_size - requested_size - 8) | 1;
                
                // This is now safe because requested_size is guaranteed to be even
                current->size_and_free = requested_size;

                new_block->next = current->next;
                current->next = (uint32_t)new_block;
            }

            // Step C: Return the usable memory space
            return (void*)((uint32_t)current + 8);
        }

        current = (struct block_header*)current->next;
    }

    return NULL;
}