/*
 * system.c - System functions for BenmoshOS
 *
 *  - syscall_dispatcher: added syscall 2 (print string from ring3 EBX pointer)
 *    and syscall 3 (yield stub for future scheduler)
 */

#include "system.h"

/* ---- Memory utilities ---- */
unsigned char *memcpy(unsigned char *dest, const uint8_t *src, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) dest[i] = src[i];
    return dest;
}

unsigned char *memset(unsigned char *dest, uint8_t val, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) dest[i] = val;
    return dest;
}

unsigned short *memsetw(unsigned short *dest, uint16_t val, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) dest[i] = val;
    return dest;
}

int strlen(const char *str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

/* strcmp: returns 1 if strings match, 0 if not.
 * NOTE: this is the OS-internal convention used throughout shell.c */
int strcmp(char* s1, char* s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') return 0;
        i++;
    }
    return 1;
}

/* ---- I/O port ---- */
uint8_t inportb(uint16_t _port) {
    unsigned char rv;
    __asm__ volatile ("inb %1, %0" : "=a" (rv) : "Nd" (_port));
    return rv;
}

uint16_t inportw(uint16_t _port) {
    uint16_t rv;
    __asm__ volatile ("inw %1, %0" : "=a" (rv) : "Nd" (_port));
    return rv;
}

void outportb(uint16_t _port, unsigned char _data) {
    __asm__ volatile ("outb %0, %1" : : "a" (_data), "Nd" (_port));
}

void outportw(uint16_t _port, uint16_t _data) {
    __asm__ volatile ("outw %0, %1" : : "a" (_data), "Nd" (_port));
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
void syscall_dispatcher(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    (void)ecx; (void)edx;

    switch (eax) {
        case 1:
            /* Syscall 1: kernel ping — just proves ring3 -> ring0 transition works */
            set_print_color(0x0D);
            print("[Kernel] Syscall 1 received from Ring3\n");
            set_print_color(0x0F);
            break;

        case 2:
            /* Syscall 2: print null-terminated string at EBX */
            if (ebx != 0) {
                print((char*)ebx);
            }
            break;

        case 3:
            /* Syscall 3: yield — placeholder for future scheduler */
            break;

        default:
            set_print_color(0x0C);
            print("[Kernel] Unknown syscall: ");
            print_int((int)eax);
            print("\n");
            set_print_color(0x0F);
            break;
    }
}
