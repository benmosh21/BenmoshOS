#include <stddef.h> // For NULL
#include <stdint.h>

// 8-byte metadata header
struct block_header {
    uint32_t size_and_free; // Top 31 bits = size, Bottom 1 bit = free flag
    struct block_header* next;          // Memory address of the next header
} __attribute__((packed));

// We pre-mapped 4MB to 8MB in the VMM. 4MB in hex is 0x400000.
// This pointer will always remember where the heap starts.
struct block_header* heap_start = (struct block_header*)0x400000;

void heap_init() {
    heap_start->size_and_free = (0x400000 - 8) | 1; // | 1 marks it as free
    heap_start->next = (struct block_header*)NULL;
}

void* kmalloc(uint32_t requested_size) {

    // Disable interrupts to prevent list corruption during multitasking
    __asm__ volatile("cli");

    // Align requested_size up to the nearest multiple of 4
    // This ensures the bottom bits (including our flag bit) are always 0
    if (requested_size % 4 != 0) {
        requested_size += 4 - (requested_size % 4);
    }

    struct block_header* current = heap_start;

    while (current != NULL) {
        // Calculate current_size by masking out the flag bit
        uint32_t current_size = current->size_and_free & ~1;

        // If we find free block at the requested size
        if ((current->size_and_free & 1) && (current_size >= requested_size)) {
            // Mark the block used
            current->size_and_free &= ~1;

            if ((current_size - requested_size) >= 24) {
                struct block_header* new_block = (struct block_header*)((uint32_t)current + requested_size + 8);

                new_block->size_and_free = (current_size - requested_size - 8) | 1;
                new_block->next = current->next;

                current->size_and_free = requested_size;
                current->next = new_block;
            }
            
            // Re-enable interrupts nefore returning success
            __asm__ volatile("sti");
            return (void*)((uint32_t)current + 8);

        }

        current = current->next;
    }


    // Re-enable interrupts before returning faliure
    __asm__ volatile("sti");
    return NULL;
}

void kfree(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    // Disable interrupts to prevent multitasking list corruption
    __asm__ volatile("cli");

    // Locate the metadata header
    struct block_header* header = (struct block_header*)((uint8_t*)ptr - 8);

    // Bounds Check
    if ((uint32_t)header < 0x400000 || (uint32_t)header >= 0x800000) {
        kernel_panic("kfree: Pointer 0x%x is outside heap bounds!", ptr);
    }

    // Double-Free Check
    if (header->size_and_free & 1) {
        kernel_panic("kfree: Double-free detected on pointer 0x%x!", ptr);
    }

    // Integrity Check
    if ((header->size_and_free & ~1) == 0) {
        kernel_panic("kfree: Corrupted heap header at 0x%x!", ptr);
    }

    // Mark the block as free
    header->size_and_free |= 1;

    // Forward Merge: Can we merge with the block AFTER us?
    struct block_header* next_block = header->next;
    if (next_block != NULL && (next_block->size_and_free & 1)) {
        uint32_t header_size = header->size_and_free & ~1;
        uint32_t next_size = next_block->size_and_free & ~1;

        header->size_and_free = (header_size + next_size + 8) | 1;
        header->next = next_block->next;
    }

    // Backward Merge: Find the block BEFORE us to merge
    struct block_header* current = heap_start;
    while (current != NULL && current->next != NULL) {

        // Did we find the block pointing to our newly freed header?
        if (current->next == header) {

            // Is the block before us also free?
            if (current->size_and_free & 1) {
                uint32_t current_size = current->size_and_free & ~1;
                uint32_t header_size = header->size_and_free & ~1;

                // Merge them!
                current->size_and_free = (current_size + header_size + 8) | 1;
                current->next = header->next;
            }

            // We found our block's neighbors and handled them. Stop searching!
            break;
        }

        // Move to the next block
        current = current->next;
    }

    // Re-enable interrupts
    __asm__ volatile("sti");
}

void* kmalloc_aligned(uint32_t size, uint32_t alignment) {
    // Make sure the alignment is a power of 2
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    // Make an offset to be the size of the alignment + one pointer-size
    uint32_t offset = alignment + sizeof(void*);
    void* raw_ptr = kmalloc(size + offset);

    if (raw_ptr == NULL) {
        return NULL;
    }

    uint32_t raw_addr = (uint32_t)raw_ptr;

    // Shift the address by the offset and then aligned it with the alignment
    uint32_t aligned_addr = (raw_addr + offset - 1) & ~(alignment - 1);

    // Put the original ptr one pointer-size before the aligned_addr
    void** hidden_tracker = (void**)(aligned_addr - sizeof(void*));
    *hidden_tracker = raw_ptr;

    return (void*)aligned_addr;
}

void kfree_aligned(void* ptr) {
    if (ptr == NULL) {
        return;
    }

    // Find the original ptr by going one pointer-size backwards from the aligned_addr
    void** hidden_tracker = (void**)((uint32_t)ptr - sizeof(void*));
    void* original_ptr = *hidden_tracker;

    kfree(original_ptr);
}