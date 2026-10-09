#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Syscall dispatcher — called from isr128 (int 0x80)
 *   eax = syscall number
 *   ebx = arg1 (string pointer for syscall 2)
 *   ecx = arg2
 *   edx = arg3 */
uint32_t syscall_dispatcher(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

#endif