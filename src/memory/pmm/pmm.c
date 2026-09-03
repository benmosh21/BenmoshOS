#include "pmm.h"

uint32_t bitmap[32768];


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
	uint32_t word = frame / 32;
	uint32_t bit = frame % 32;
	return !(bitmap[word] & (1 << bit));
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

	// Set defult values for bitmap (fill bitmap with 1s)
	for (int i = 0; i < sizeof(bitmap) / sizeof(uint32_t); i++) {
		bitmap[i] = 0xFFFFFFFF; // Set all bits to 1 (all pages are allocated)
	}

	// Read the BIOS Memory Map
	struct memorymap_entry* entry = (struct memorymap_entry*)0x5000; // Assuming the memory map is stored at 0x5000

	while (entry->length > 0) {
		// Type 1 means Usable RAM
		if (entry->type == 1) {

			uint32_t length_32 = (uint32_t)(entry->length);
			uint32_t base_32 = (uint32_t)(entry->base);

			uint32_t starting_frame_index = base_32 / 4096; // Calculate the starting frame index
			uint32_t num_frames = length_32 / 4096; // Calculate the number of frames in this region

			// Free the frames in this usable memory region
			for (uint32_t i = 0; i < num_frames; i++) {
				uint32_t current_frame = starting_frame_index + i;
				uint32_t bucket = current_frame / 32;		 // Each uint32_t in the bitmap represents 32 frames
				uint32_t bit_position = current_frame % 32;	 // Calculate the bit offset within the uint32_t

				// Clear the bit to mark the frame as free
				bitmap[bucket] &= ~(1U << bit_position);
			}

		}
		entry++; // Move to the next entry
	}

	// Protect the first 1 MB of memory (BIOS, VGA buffer, Bootloader)
	pmm_reserve_block(0x00000000, 0x100000); // Mark the first 1 MB as allocated

	// Protect the memory used by the kernel
	extern uint32_t kernel_end; // This symbol is defined in the linker script
	uint32_t kernel_end_address = (uint32_t)&kernel_end;

	if (kernel_end_address > 0x100000) {
		// If the kernel is larger than 1 MB, we need to reserve the additional memory it occupies
		uint32_t spill_size = kernel_end_address - 0x100000; // Calculate how much memory spills beyond the first 1 MB
		pmm_reserve_block(0x100000, spill_size);
	}
}


/**
 * @brief: Allocates a single 4KB physical frame.
 *
 * Performs a linear search across the bitmap for the first available bit (0),
 * marks it as allocated (1), and calculates its raw physical base address.
 *
 * @return: uint32_t The 4KB-aligned physical base address of the frame,
 *                  or 0 if the system is completely out of physical memory.
 */
uint32_t pmm_alloc_block() {
	for (uint32_t current_bucket = 0; current_bucket < 32768; current_bucket++) {
		// Skip full buckets
		if (bitmap[current_bucket] != 0xFFFFFFFF) {

			// Check each bit in the bucket
			for (int i = 0; i < 32; i++) {
				if ((bitmap[current_bucket] & (1U << i)) == 0) {

					// 1. Mark the bit as used
					bitmap[current_bucket] |= (1U << i);

					// 2. Calculate the frame index
					uint32_t allocated_frame = (32 * current_bucket) + i;

					// 3. Return the actual memory address
					return (allocated_frame * 4096);
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
 * specified physical address across the requested number of frames.
 *
 * @param: physical_address The physical base address to begin releasing (must be 4KB aligned).
 * @param: num_frames The count of consecutive 4KB frames to mark as free.
 */
void pmm_free_block(uint32_t physical_address, uint32_t num_frames) {
	uint32_t starting_frame = physical_address / 4096;

	for (uint32_t i = 0; i < num_frames; i++) {
		// Find the current frame
		uint32_t current_frame = starting_frame + i;

		// Find the bucker and bit
		uint32_t bucket = current_frame / 32;
		uint32_t bit_position = current_frame % 32;

		// Clear the bit to 0 (Free)
		bitmap[bucket] &= ~(1U << bit_position);
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
	uint32_t num_frames = (size + 4095) / 4096; // Round up to the nearest frame

	// Go over each frame and set them to free=0
	for (uint32_t i = 0; i < num_frames; i++) {
		uint32_t current_frame = starting_frame + i;
		uint32_t bucket = current_frame / 32;
		uint32_t bit_position = current_frame % 32;

		bitmap[bucket] |= (1U << bit_position); // Set the bit to 1 (Allocated)
	}
}

uint32_t pmm_alloc_contiguous(uint32_t count) {
	if (count == 0) return 0;

	uint32_t max_frames = 32768 * 32;

	// Loop through physical frames, stopping early enough so we don't read out of bounds
	for (uint32_t i = 0; i <= max_frames - count; i++) {
		int found = 1;

		// Check if 'count' consecutive frames are free
		for (uint32_t j = 0; j < count; j++) {
			if (!pmm_is_frame_free(i + j)) {
				found = 0;
				i += j; // Optimization: skip ahead past the used frame
				break;
			}
		}

		// If we found an unbroken sequence, allocate and return
		if (found) {
			for (uint32_t j = 0; j < count; j++) {
				uint32_t current_frame = i + j;
				uint32_t word = current_frame / 32;
				uint32_t bit = current_frame % 32;

				bitmap[word] |= (1U << bit);
			}
			return i * 4096;
		}
	}

	return 0; // Out of contiguous memory
}