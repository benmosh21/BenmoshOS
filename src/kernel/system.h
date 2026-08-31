#ifndef __SYSTEM_H
#define __SYSTEM_H

#include <stdint.h>
#include "../drivers/print/print.h"
#include "../libc/stdlib.h"
#include "../libc/strings.h"

extern uint32_t gdt_start;
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

/* I/O ports */
uint8_t  inportb(uint16_t _port);
uint16_t inportw(uint16_t _port);
void     outportb(uint16_t _port, unsigned char _data);
void     outportw(uint16_t _port, uint16_t _data);


/* Syscall dispatcher — called from isr128 (int 0x80)
 *   eax = syscall number
 *   ebx = arg1 (string pointer for syscall 2)
 *   ecx = arg2
 *   edx = arg3 */
uint32_t syscall_dispatcher(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

#endif
