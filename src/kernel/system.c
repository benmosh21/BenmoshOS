/*
 * system.c - System functions for BenmoshOS
 *
 * FIXES:
 *  - init_tss(): TSS access byte changed 0xE9 -> 0x89
 *    (0xE9 = DPL3 call gate, wrong; 0x89 = 32-bit TSS, present, DPL0, correct)
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

/* ---- TSS ---- */
tss_entry_t tss_entry;

void flush_tss() {
    /* 0x28 = GDT offset of the TSS descriptor (6th descriptor * 8 bytes = 48 = 0x30)
     * With RPL=0 in LTR, selector = 0x28 maps to the 6th 8-byte entry.
     * GDT layout: [0]=null [1]=kcode(0x08) [2]=kdata(0x10)
     *             [3]=ucode(0x18) [4]=udata(0x20) [5]=tss(0x28) */
    __asm__ volatile ("ltr %%ax" : : "a" (0x28));
}

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

void init_tss() {
    struct gdt_ptr current_gdt;
    __asm__ volatile ("sgdt %0" : "=m" (current_gdt));

    uint8_t* gdt_start = (uint8_t*)current_gdt.base;

    uint32_t base  = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry_t) - 1;  /* limit is inclusive */

    /* GDT[5] starts at byte offset 40 (5 * 8) */
    /* Byte layout of an 8-byte GDT descriptor:
     *  [0..1] limit[0..15]
     *  [2..4] base[0..23]
     *  [5]    access byte
     *  [6]    flags | limit[16..19]
     *  [7]    base[24..31] */

    gdt_start[40 + 0] =  limit        & 0xFF;
    gdt_start[40 + 1] = (limit >> 8)  & 0xFF;

    gdt_start[40 + 2] =  base         & 0xFF;
    gdt_start[40 + 3] = (base  >> 8)  & 0xFF;
    gdt_start[40 + 4] = (base  >> 16) & 0xFF;
    gdt_start[40 + 7] = (base  >> 24) & 0xFF;

    /* FIX: was 0xE9 (wrong — that encodes DPL=3 and a call-gate-like type).
     *      Must be 0x89 = Present(1) | DPL=00 | Type=1001 (32-bit TSS available) */
    gdt_start[40 + 5] = 0x89;
    gdt_start[40 + 6] = 0x00;

    /* Zero the TSS struct */
    uint8_t* p = (uint8_t*)&tss_entry;
    for (uint32_t i = 0; i < sizeof(tss_entry_t); i++) p[i] = 0;

    /* Ring0 stack: used when the CPU switches from ring3 -> ring0 on a syscall/IRQ */
    tss_entry.ss0  = 0x10;       /* Kernel data segment */
    tss_entry.esp0 = 0x90000;    /* Top of a safe 4KB ring0 stack */
    tss_entry.iomap_base = sizeof(tss_entry_t);

    flush_tss();
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
