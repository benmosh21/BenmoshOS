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
    struct block_header* current = first_header;

    while (current != NULL) {

        if ((current->size_and_free & 1) && ((current->size_and_free & ~1) >= requested_size)) {

            // Step A: Mark the block as USED
            current->size_and_free &= ~1;

            // Step B: Carve the block if it's bigger than we need
            if ((current_size - requested_size) >= 24) {
                // 1. Create the new header
                struct block_header* new_block = (struct block_header*)((uint32_t)current + 8 + requested_size);

                // 2. Set the new block's size and free flag
                new_block->size_and_free = (current_size - requested_size - 8) | 1;

                // 3. Update current block's size (and keep free flag 0)
                current->size_and_free = requested_size;

				// 4. Update the linked list pointers
				new_block->next = current->next;
				current->next = (uint32_t)new_block;
            }
            else {
				// If we can't carve, just mark the whole block as used
				current->size_and_free &= ~1; // Clear the free flag
            }


            // Step C: Return the usable memory space to the program
            return (void*)((uint32_t)current + 8);
        }

        // Move to the next block in the linked list
        current = (struct block_header*)current->next;
    }

    // We reached the end of the list and found nothing
    return NULL;
}