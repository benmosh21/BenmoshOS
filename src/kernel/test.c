#include "../memory/pmm/pmm.h"
#include "../memory/vmm/vmm.h"
#include "../memory/heap/heap.h"
#include "../drivers/print/print.h"
#include <stdint.h>


void test_memory() {
    print("--- STARTING COMPREHENSIVE MEMORY TESTS ---\n");

    // ==========================================
    // TEST 1: PMM (Physical Memory Manager)
    // ==========================================
    print("\n[PMM TEST]\n");
    uint32_t frame1 = pmm_alloc_block();
    uint32_t frame2 = pmm_alloc_block();
    print("Allocated Frame 1: "); print_hex(frame1); print("\n");
    print("Allocated Frame 2: "); print_hex(frame2); print("\n");

    // 1a. Test Freeing: If we free frame1, the very next allocation should grab it again.
    pmm_free_block(frame1, 1);
    uint32_t frame3 = pmm_alloc_block();
    print("Freed Frame 1, Allocated Frame 3: "); print_hex(frame3); print("\n");

    if (frame3 == frame1) {
        print("PMM Free Test: SUCCESS\n");
    }
    else {
        print("PMM Free Test: FAILED\n");
    }

    // 1b. Test Reserving: We will reserve the frame exactly after frame2.
    // The next allocation should skip it and jump to the one after.
    uint32_t target_reserve = frame2 + 4096;
    pmm_reserve_block(target_reserve, 4096);
    uint32_t frame4 = pmm_alloc_block();

    print("Reserved block at "); print_hex(target_reserve); print("\n");
    print("Allocated Frame 4: "); print_hex(frame4); print("\n");

    if (frame4 != target_reserve && frame4 == target_reserve + 4096) {
        print("PMM Reserve Test: SUCCESS\n");
    }
    else {
        print("PMM Reserve Test: FAILED\n");
    }

    // ==========================================
    // TEST 2: VMM (Virtual Memory Manager)
    // ==========================================
    print("\n[VMM TEST]\n");
    uint32_t vmm_phys_frame = pmm_alloc_block();
    uint32_t test_virtual_address = 0x80000000; // The 2GB mark in high memory

    // 2a. Map a raw physical frame to a high virtual address (Set Present + Read/Write flags)
    vmm_map_page(vmm_phys_frame, test_virtual_address, 3);

    // 2b. Write to the virtual address. If the scratchpad failed, this will crash the OS.
    uint32_t* vmm_ptr = (uint32_t*)test_virtual_address;
    *vmm_ptr = 0xCAFEBABE;

    // 2c. Read it back
    if (*vmm_ptr == 0xCAFEBABE) {
        print("VMM Mapping & Scratchpad Test: SUCCESS\n");
    }
    else {
        print("VMM Mapping Test: FAILED\n");
    }

    // ==========================================
    // TEST 3: HEAP (Malloc and Free)
    // ==========================================
    print("\n[HEAP TEST]\n");

    // 3a. Allocate three consecutive blocks
    void* ptr_A = malloc(32);
    void* ptr_B = malloc(32);
    void* ptr_C = malloc(32);

    print("Malloc A: "); print_hex((uint32_t)ptr_A); print("\n");
    print("Malloc B: "); print_hex((uint32_t)ptr_B); print("\n");
    print("Malloc C: "); print_hex((uint32_t)ptr_C); print("\n");

    // 3b. Test Heap Reuse (Free the middle block)
    free(ptr_B);
    void* ptr_D = malloc(32); // This should fill the hole left by B
    print("Freed B, Malloc D: "); print_hex((uint32_t)ptr_D); print("\n");

    if (ptr_D == ptr_B) {
        print("Heap Reuse Test: SUCCESS\n");
    }
    else {
        print("Heap Reuse Test: FAILED\n");
    }

    // 3c. Test Heap Defragmentation (Coalescing)
    // We free all three adjacent blocks. If the while loop in your free() logic works,
    // it will merge them all back into the massive 4MB main block.
    free(ptr_A);
    free(ptr_D); // D is currently sitting where B used to be
    free(ptr_C);

    void* ptr_massive = malloc(256);
    print("Freed all, Malloc Massive: "); print_hex((uint32_t)ptr_massive); print("\n");

    if (ptr_massive == ptr_A) {
        print("Heap Coalescing Test: SUCCESS\n");
    }
    else {
        print("Heap Coalescing Test: FAILED\n");
    }

    print("\n--- ALL TESTS COMPLETE ---\n");
}

// heap overflow exploit
void test_heap_overflow() {
    print("\n--- INITIATING HEAP OVERFLOW EXPLOIT ---\n");

    // 1. Allocate two adjacent blocks.
    // We ask for 16 bytes. The heap actually carves out 24 bytes (8 for header + 16 for data).
    uint32_t* block_A = (uint32_t*)malloc(16);
    uint32_t* block_B = (uint32_t*)malloc(16);

    print("Block A usable memory starts at: "); print_int((uint32_t)block_A); print("\n");
    print("Block B header should be at:     "); print_int((uint32_t)block_A + 16); print("\n");

    // 2. Fill Block A with normal data (4 integers = 16 bytes)
    block_A[0] = 0xAAAAAAAA;
    block_A[1] = 0xAAAAAAAA;
    block_A[2] = 0xAAAAAAAA;
    block_A[3] = 0xAAAAAAAA; // This is the absolute limit of Block A's valid memory

    print("Writing normal data to Block A... OK\n");

    // 3. THE OVERFLOW
    // We keep writing past the 16 bytes we were given.
    // block_A[4] lines up perfectly with Block B's 'size_and_free' variable.
    // block_A[5] lines up perfectly with Block B's 'next' pointer.

    print("Overflowing Block A to corrupt Block B's metadata...\n");

    // We overwrite Block B's size to look like a massive USED block (bottom bit is 0)
    block_A[4] = 0x99999998;

    // We overwrite Block B's NEXT pointer to a malicious, unmapped memory address
    block_A[5] = 0xDEADBEEF;

    print("Metadata corrupted successfully.\n");

    // 4. TRIGGER THE CRASH
    // We call malloc again. Malloc will start at the beginning of the heap and walk the 
    // linked list. It will read Block A's header, jump to Block B's header, see that Block B 
    // is "used", and try to jump to Block B's 'next' pointer to keep searching.
    // It will blindly jump to 0xDEADBEEF.

    print("Calling malloc to trigger list traversal...\n");

    void* trigger = malloc(16);

    // If the kernel survives, the exploit failed. But it won't survive.
    print("If you see this, the exploit failed!\n");
}

// heap overflow exploit
void test_data_overwrite() {
    print("\n--- HEAP DATA OVERWRITE EXPLOIT ---\n");

    // 1. Allocate a vulnerable buffer
    char* vulnerable_buffer = (char*)malloc(16);

    // 2. Allocate the target integer immediately after it
    int* target_variable = (int*)malloc(4);
    *target_variable = 1000; // Set its initial value

    print("Target variable is initially: "); print_int(*target_variable); print("\n");

    // 3. THE EXPLOIT
    // vulnerable_buffer has 16 bytes of space.
    // Right after those 16 bytes is the 8-byte metadata header for target_variable.
    // Right after that header is the actual data for target_variable.
    // Total distance = 16 + 8 = 24 bytes.

    print("Writing past the buffer to corrupt the adjacent variable...\n");

    // We write exactly to the 24th byte, which is where the integer's data lives!
    int* corruptor_pointer = (int*)(vulnerable_buffer + 24);
    *corruptor_pointer = 9999; // Maliciously overwrite the integer

    // 4. Verify the exploit
    print("Target variable is now: "); print_int(*target_variable); print("\n");

    if (*target_variable == 9999) {
        print("Exploit SUCCESS: Data successfully hijacked!\n");
    }
}

// stack overflow exploit
void test_stack_overflow() {
    print("\n--- STACK OVERFLOW EXPLOIT ---\n");

    // 1. Declare local variables on the stack
    // The compiler will place 'target' and 'buffer' right next to each other.
    volatile int target = 1000;
    char buffer[16];

    print("Target is initially: "); print_int(target); print("\n");

    // 2. THE EXPLOIT
    // We intentionally write past the 16th byte of the array.
    // This bleeds directly into the memory space of 'target'.
    print("Overflowing the stack buffer...\n");
    for (int i = 0; i < 20; i++) {
        buffer[i] = 'A'; // 'A' is 0x41 in hexadecimal
    }

    // 3. Verify the exploit
    // If it worked, 'target' will no longer be 1000. It will be corrupted by the 'A's.
    print("Target is now: "); print_int(target); print("\n");

    if (target != 1000) {
        print("Exploit SUCCESS: Stack variable hijacked!\n");
    }
}