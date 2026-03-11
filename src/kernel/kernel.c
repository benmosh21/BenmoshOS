#include "../drivers/print/print.h"
#include "../memory/pmm/pmm.h"
#include "../memory/vmm/vmm.h"
#include "../memory/heap/heap.h"
#include "system.h"


void main() {
    // --- HARDWARE DEBUG: Print 'C' (Red background) ---
    // Cast the physical VGA memory address to a pointer and force a write
    *((char*)0xb8004) = 'C';
    *((char*)0xb8005) = 0x4F;

    // --- FREEZE THE OS SO WE CAN SEE THE LETTERS! ---
   // while (1);

    // Anything below this will NOT run right now.
    // 1. Basic Screen Setup
    screen_clear();
    //set_print_color(0x0F); // White text
    print("Booting BenmoshOS...\n");

    //// 2. Memory Subsystems
    //print("Initializing PMM...\n");
    //pmm_init();
    //
    //print("Initializing VMM...\n");
    //vmm_init();
    //
    //print("Initializing Heap...\n");
    //heap_init();
    //
    //print("Enabling Dynamic History...\n");
    //enable_dynamic_history(1000);
    //
    //// 3. Security Subsystems (Phase 2)
    //print("Initializing TSS...\n");
    //init_tss();
    //
    //set_print_color(0x0A); // Light Green
    //print("TSS Initialized successfully!\n");
    //set_print_color(0x0F); // Back to White
    //
    //print("\n--- FREEZE TEST ACTIVE ---\n");
    //print("If the OS freezes here without rebooting, Phase 1 is perfect!\n");
    //
    //// ==========================================
    //// THE FREEZE: The CPU will get stuck here.
    //// ==========================================
    //while (1);
    //
    //// ==========================================
    //// Anything below this line will NOT execute yet.
    //// ==========================================
    //
    //print("Jumping to User Mode...\n");
   //// jump_usermode(my_user_program);
    //
    //print("If you see this, the jump failed.\n");
}