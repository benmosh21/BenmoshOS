#include "pmm.h"
#include "../../kernel/panic/panic.h"

/*
 *
 * The pmm (physical memory managment) is devided to 2 levels. The fitst level 
 * is big bitmap of 32768 32bits entries each entries point to one physical address.
 * The second level is 1024 32bits enties that points if a block of 32 physical address.
 * 
*/

// Base bitmap (1,048,576 bits (1MB) -> 4GB RAM)
uint32_t bitmap[32768];

// Summary Bitmap (1024 bits)
// 0 = Block has at least one free frame. 1 = Block is completely full (all frames allocated)
uint32_t summary_bitmap[1024];

uint32_t free_frame_count = 0;
uint32_t total_frames = 1048576; // 4GB / 4KB = 1,048,576 frames

/**
 * @brief: Checks if a specific 4KB physical memory frame is available.
 *
 * Performs a bitwise inspection on the PMM allocation bitmap to determine
 * if the designated physical frame index is free or currently marked as used.
 *
 * @param: frame The 0-indexed physical frame number (Physical Address / 4096).
 * @return int 1 if the frame is free (bit is 0), 0 if allocated/reserved (bit is 1).
 */
int pmm_is_frame_free(uint32_t frame) {
    if (frame >= total_frames) return 0;
    uint32_t word = frame / 32;
    uint32_t bit = frame % 32;
    return !(bitmap[word] & (1U << bit));
}

/**
 * @brief: Internal helper to mark a physical frame as used.
 * 
 * Sets the corresponding bit in the base bitmap to 1. If the 32-frame bucket 
 * becomes completely full, it updates the summary bitmap to accelerate future allocations.
 * 
 * @param: frame The 0-indexed physical frame number.
 */
static void pmm_mark_used(uint32_t frame) {
    if (frame >= total_frames) return;
    uint32_t word = frame / 32;
    uint32_t bit = frame % 32;
    bitmap[word] |= (1U << bit);

    // Update the summary bitmap if the entire word is now full
    if (bitmap[word] == 0xFFFFFFFF) {
        uint32_t summary_word = word / 32;
        uint32_t summary_bit = word % 32;
        summary_bitmap[summary_word] |= (1U << summary_bit);
    }
}

/**
 * @brief: Internal helper to mark a physical frame as free.
 * 
 * Clears the corresponding bit in the base bitmap to 0. Automatically flags 
 * the summary bitmap to indicate that this bucket now has available capacity.
 * 
 * @param: frame The 0-indexed physical frame number.
 */
static void pmm_mark_free(uint32_t frame) {
    if (frame >= total_frames) return;
    uint32_t word = frame / 32;
    uint32_t bit = frame % 32;
    bitmap[word] &= ~(1U << bit);

    // Update the summary bitmap since the word is guaranteed to have space
    uint32_t summary_word = word / 32;
    uint32_t summary_bit = word % 32;
    summary_bitmap[summary_word] &= ~(1U << summary_bit);
}

/**
 * @brief: Initializes the Physical Memory Manager and populates the bitmap.
 *
 * Defaults the entire physical memory bitmap to fully allocated (0xFFFFFFFF).
 * Parses the BIOS INT 0x15, E820 memory map handed off at 0x5000 to identify
 * usable physical RAM regions and clears their respective bits to 0.
 * Automatically marks the first 1MB of physical memory (BIOS, VGA, Real Mode
 * artifacts) and the resident kernel ELF binary space as reserved.
 *
 * @note: Must be called before virtual memory paging or heap systems are activated.
 */
void pmm_init() {

    // Set default values for bitmap (fill bitmap with 1s)
    for (int i = 0; i < sizeof(bitmap) / sizeof(uint32_t); i++) {
        bitmap[i] = 0xFFFFFFFF; 
    }
    for (int i = 0; i < sizeof(summary_bitmap) / sizeof(uint32_t); i++) {
        summary_bitmap[i] = 0xFFFFFFFF; 
    }
    free_frame_count = 0; 

    // Read the BIOS Memory Map
    struct memorymap_entry* entry = (struct memorymap_entry*)0x5000; 

    while (entry->length > 0) {
        // Type 1 means Usable RAM
        if (entry->type == 1) {
            uint32_t length_32 = (uint32_t)(entry->length);
            uint32_t base_32 = (uint32_t)(entry->base);

            uint32_t starting_frame_index = base_32 / 4096; 
            uint32_t num_frames = length_32 / 4096; 

            // Free the frames in this usable memory region
            for (uint32_t i = 0; i < num_frames; i++) {
                uint32_t frame = starting_frame_index + i;
                if (frame < total_frames) {
                    pmm_mark_free(frame);
                    free_frame_count++;
                }
            }
        }
        entry++; 
    }

    // Protect the first 1 MB of memory (BIOS, VGA buffer, Bootloader)
    pmm_reserve_block(0x00000000, 0x100000); 

    // Protect the memory used by the kernel
    extern uint32_t kernel_end; 
    uint32_t kernel_end_address = (uint32_t)&kernel_end;

    if (kernel_end_address > 0x100000) {
        uint32_t spill_size = kernel_end_address - 0x100000; 
        pmm_reserve_block(0x100000, spill_size);
    }

    // Protect the Kernel Heap (4MB to 8MB)
    pmm_reserve_block(0x00400000, 0x00400000);
}

/**
 * @brief: Allocates a single 4KB physical frame.
 *
 * Utilizes a hierarchical O(1)-approximate scan. Evaluates the summary bitmap 
 * to bypass fully allocated 128KB memory blocks, drastically reducing lookup 
 * times before falling back to base bitmap bit-scanning.
 *
 * @return: uint32_t The 4KB-aligned physical base address of the frame,
 *                  or 0 if the system is completely out of physical memory.
 */
uint32_t pmm_alloc_frame() {
    // Hierarchical Scan: Jump by 1024 frames at a time
    for (uint32_t i = 0; i < 1024; i++) {
        // If summary word is 0xFFFFFFFF, all 1024 frames underneath are full.
        if (summary_bitmap[i] != 0xFFFFFFFF) {
            // Found a summary bucket with space. Scan its 32 base words.
            for (int j = 0; j < 32; j++) {
                if ((summary_bitmap[i] & (1U << j)) == 0) {
                    // Found the exact base word with space. Scan its 32 bits.
                    uint32_t word = (i * 32) + j;

                    for (uint32_t k = 0; k < 32; k++) {
                        if ((bitmap[word] & (1U << k)) == 0) {
                            // Calculate the absolute physical frame index
                            uint32_t frame = (word * 32) + k;
                            pmm_mark_used(frame);
                            free_frame_count--;

                            return frame * 4096;
                        }
                    }
                }
            }
        }
    }
    
    return 0; // Return 0 if we are out of memory
}

/**
 * @brief: Frees a contiguous series of physical memory frames.
 *
 * Clears the allocation bits (sets to 0) in the bitmap starting from the
 * specified physical address across the requested number of frames. Protects
 * against double-free corruption by validating frame ownership prior to clearing.
 *
 * @param: physical_address The physical base address to begin releasing (must be 4KB aligned).
 * @param: num_frames The count of consecutive 4KB frames to mark as free.
 */
void pmm_free_block(uint32_t physical_address, uint32_t num_frames) {
    uint32_t starting_frame = physical_address / 4096;

    if (pmm_is_frame_free(starting_frame)) {
        kernel_panic("[PMM] Warning: Attempted to free an already free frame at %x", physical_address);
		return;
	}

    for (uint32_t i = 0; i < num_frames; i++) {
        uint32_t current_frame = starting_frame + i;

        // Double-Free Guard
        if (pmm_is_frame_free(current_frame)) {
            continue; // Already free; do not artificially inflate the frame count
        }

        pmm_mark_free(current_frame);
        free_frame_count++;
    }
}

/**
 * @brief: Marks a specified physical memory range as reserved in the bitmap.
 *
 * Sets the allocation bits (1) for all frames covered by the given base address
 * and byte size, ensuring the allocator will not vend these memory regions.
 *
 * @param: physical_address Starting physical address to protect.
 * @param: size Total span of bytes to reserve (automatically rounded up to 4KB).
 */
void pmm_reserve_block(uint32_t physical_address, uint32_t size) {
    uint32_t starting_frame = physical_address / 4096;
    uint32_t num_frames = (size + 4095) / 4096; 

    for (uint32_t i = 0; i < num_frames; i++) {
        uint32_t current_frame = starting_frame + i;

        // Only reserve if it is currently free
        if (pmm_is_frame_free(current_frame)) {
            pmm_mark_used(current_frame);
            free_frame_count--;
        }
    }
}

/**
 * @brief: Retrieves current physical memory usage statistics (total, free and used).
 *
 * @param: pointers to caller total_frames, total_free_frames and total_used_frame
 */
void pmm_get_stats(uint32_t* out_total, uint32_t* out_free, uint32_t* out_used) {
    if (out_total) *out_total = total_frames;
    if (out_free)  *out_free = free_frame_count;
    if (out_used)  *out_used = total_frames - free_frame_count;
}