#include "syscall.h"
#include "../../drivers/print/print.h"
#include "../../drivers/keyboard/keyboard.h"

/* ---- Syscall dispatcher ----
 *
 * Called from isr128 (int 0x80) with:
 *   eax = syscall number
 *   ebx = arg1  (string pointer for syscall 2)
 *   ecx = arg2
 *   edx = arg3
 *
 * Some syscalls use pointers as input, the pointers come from ring3.
 * Because the OS uses an identity-mapped flat 4GB model that puts the kernel and user
 * in the same page directory, the pointer will be valid in ring0
 */
uint32_t syscall_dispatcher(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)ecx; (void)edx;

    switch (eax) {
        case 1:
            /* Syscall 1: kernel ping — just proves ring3 -> ring0 transition works */
            set_print_color(0x0D);
            puts("[Kernel] Syscall 1 received from Ring3\n");
            set_print_color(0x0F);
            break;

        case 2:
            /* Syscall 2: print null-terminated string at EBX */
            if (ebx != 0) {
                puts((char*)ebx);
            }
            break;

        case 3:
            /* Syscall 3: yield — placeholder for future scheduler */
            break;

        case 4: {
            /* Syscall 4: Input string from keyboard with max limit configuration */
            static char input_buffer[256];
            uint32_t max_len = ebx;

            if (max_len == 0) return 0;
            if (max_len > 255) max_len = 255; // Hardware safety cap bounds

            uint32_t index = 0;

            // Synchronous polling loop reading inputs safely until limit is achieved
            while (index < (max_len - 1)) {
                // Halt CPU until next interrupt (keyboard) and re-enable interrupts for ISR handling
                __asm__ volatile ("sti\n\thlt\n\tcli");

                char c = keyboard_pop_char();
                if (c == 0) {
                    task_yield(); // No input yet, yield to other tasks
                    continue;
                }

                if (c == '\n') {
                    input_buffer[index++] = '\n';
                    puts("\n");
                    break;
                } 
                else if (c == '\b') {
                    if (index > 0) {
                        index--;
                        char bs_str[4] = {'\b', ' ', '\b', 0};
                        puts(bs_str);
                    }
                } 
                else {
                    input_buffer[index++] = c;
                    char echo_str[2] = {c, 0};
                    puts(echo_str);
                }
            }
            
            
            input_buffer[index] = '\0';
            
            // Return address memory pointer targeting the completed kernel buffer inside EAX
            return (uint32_t)input_buffer;
        }

        default:
            set_print_color(0x0C);
            puts("[Kernel] Unknown syscall: ");
            print_int((int)eax);
            puts("\n");
            set_print_color(0x0F);
            break;
    }
    return 0;
}