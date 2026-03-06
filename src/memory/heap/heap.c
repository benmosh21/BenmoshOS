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

void heap_init() {
    // The heap is 4MB long (0x400000 bytes)
    // Subtract 8 bytes for the header itself
    first_header->size_and_free = (0x400000 - 8) | 1; // | 1 marks it as free
    first_header->next = (uint32_t)NULL;
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

void free(void* ptr) {
    // If the user passes a null pointer, just ignore it.
    if (ptr == NULL) {
        return;
    }

    // Step 1: Find the hidden metadata header.
    // We cast ptr to an integer, subtract 8 bytes, and cast it back to a header pointer.
    struct block_header* target_block = (struct block_header*)((uint32_t)ptr - 8);

    // Step 2: Mark the block as free by setting the lowest bit to 1 using bitwise OR.
    target_block->size_and_free |= 1;

    // Step 3: Coalesce (Defragment) the heap.
    // Because we only have a "next" pointer (a singly linked list), we start from the beginning 
    // and scan the entire heap to merge any neighboring free blocks.
    struct block_header* current = first_header;

    while (current != NULL && current->next != 0) {
        struct block_header* next_block = (struct block_header*)current->next;

        // Check if BOTH the current block AND the next block are free (bottom bit is 1)
        if ((current->size_and_free & 1) && (next_block->size_and_free & 1)) {

            // Mask out the bottom bit to get the raw sizes
            uint32_t current_size = current->size_and_free & ~1;
            uint32_t next_size = next_block->size_and_free & ~1;

            // Merge them. 
            // The new size is current size + next size + 8 bytes (the header we are absorbing).
            current->size_and_free = (current_size + next_size + 8) | 1;

            // Update the linked list to bypass the absorbed block.
            current->next = next_block->next;

            // IMPORTANT: Do not move 'current' forward yet. 
            // The newly merged block might need to merge with the next block after it!
        }
        else {
            // Only move to the next block if no merge happened.
            current = (struct block_header*)current->next;
        }
    }
}