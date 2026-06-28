#ifndef __SYSTEM_H
#define __SYSTEM_H

#include <stdint.h>

/* Forward declarations from print.h to break circular include */
void print(char *str);
void print_int(int n);
void print_hex(int n);
void set_print_color(uint16_t color);

/* Memory utilities */
extern unsigned char  *memcpy(unsigned char *dest, const uint8_t *src, uint32_t count);
extern unsigned char  *memset(unsigned char *dest, uint8_t val, uint32_t count);
extern unsigned short *memsetw(unsigned short *dest, uint16_t val, uint32_t count);
int strlen(const char *str);
int strcmp(char* s1, char* s2);

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
void syscall_dispatcher(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

#endif
