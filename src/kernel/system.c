/*
 * system.c - System functions for BenmoshOS
 *
 *  - syscall_dispatcher: added syscall 2 (print string from ring3 EBX pointer)
 *    and syscall 3 (yield stub for future scheduler)
 */

#include "system.h"


void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {

    // Get the location of gdt_start so i can set the bits in the location for the GDT entry
    uint8_t* gdt_entry = (uint8_t*)&gdt_start + 8 * num;

    gdt_entry[0] = (limit & 0xFF);                              // Limit (7:0)
    gdt_entry[1] = (limit >> 8) & 0xFF;                         // Limit (15:8)
    gdt_entry[2] = (base & 0xFF);                               // Base (7:0)
    gdt_entry[3] = (base >> 8) & 0xFF;                          // Base (15:8)
    gdt_entry[4] = (base >> 16) & 0xFF;                         // Base (23:16)
    gdt_entry[5] = access;                                      // Accsess Byte
    gdt_entry[6] = (gran & 0xF << 4) | (limit >> 16) & 0xF;     // Flags
    gdt_entry[7] = (base >> 24) & 0xFF;                         // Base (31:24)

}


/* ---- I/O port ---- */
uint8_t inportb(uint16_t _port) {
    unsigned char rv;
    __asm__ volatile ("{inb %1, %0 | in %0, %1}" : "=a" (rv) : "Nd" (_port));
    return rv;
}

uint16_t inportw(uint16_t _port) {
    uint16_t rv;
    __asm__ volatile ("{inw %1, %0 | in %0, %1}" : "=a" (rv) : "Nd" (_port));
    return rv;
}

void outportb(uint16_t _port, unsigned char _data) {
    __asm__ volatile ("{outb %0, %1 | out %1, %0}" : : "a" (_data), "Nd" (_port));
}

void outportw(uint16_t _port, uint16_t _data) {
    __asm__ volatile ("{outw %0, %1 | out %1, %0}" : : "a" (_data), "Nd" (_port));
}

/* ---- Syscall dispatcher ----
 *
 * Called from isr128 (int 0x80) with:
 *   eax = syscall number
 *   ebx = arg1  (string pointer for syscall 2)
 *   ecx = arg2
 *   edx = arg3
 *
 * IMPORTANT: The string pointer in EBX comes from ring3 virtual address space.
 * Since we use an identity-mapped flat 4GB model (kernel + user share the same
 * page directory), the pointer is valid in ring0 as well.
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

            // Replicate normal lowercase/symbol layout scancodes matching driver specification
            static const char scancode_ascii_map[128] = {
                0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
                '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
                0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
                'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
            };

            // Synchronous polling loop reading inputs safely until limit is achieved
            while (index < (max_len - 1)) {
                // Poll the PS/2 Keyboard Status Register (Port 0x64), bit 0 indicates data ready
                if ((inportb(0x64) & 1) != 0) {
                    uint8_t scancode = inportb(0x60);

                    // Skip break codes (key release events have bit 7 active)
                    if (scancode & 0x80) continue;

                    char c = (scancode < 128) ? scancode_ascii_map[scancode] : 0;

                    if (c == '\n') {
                        char nl_str[2] = {'\n', 0};
                        puts(nl_str);
                        break;
                    } 
                    else if (c == '\b') {
                        if (index > 0) {
                            index--;
                            char bs_str[4] = {'\b', ' ', '\b', 0};
                            puts(bs_str);
                        }
                    } 
                    else if (c != 0) {
                        input_buffer[index++] = c;
                        char echo_str[2] = {c, 0};
                        puts(echo_str);
                    }
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
