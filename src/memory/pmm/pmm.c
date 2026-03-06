#include "pmm.h"

uint32_t bitmap[32768];

void pmm_init() {

	// 1. Set defult values for bitmap (fill bitmap with 1s)
	for (int i = 0; i < sizeof(bitmap) / sizeof(uint32_t); i++) {
		bitmap[i] = 0xFFFFFFFF; // Set all bits to 1 (all pages are allocated)
	}

	// 2. Read the BIOS Memory Map
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

void pmm_reserve_block(uint32_t physical_address, uint32_t size) {
	uint32_t starting_frame = physical_address / 4096;
	uint32_t num_frames = (size + 4095) / 4096; // Round up to the nearest frame

	for (uint32_t i = 0; i < num_frames; i++) {
		uint32_t current_frame = starting_frame + i;
		uint32_t bucket = current_frame / 32;
		uint32_t bit_position = current_frame % 32;

		bitmap[bucket] |= (1U << bit_position); // Set the bit to 1 (Allocated)
	}
}