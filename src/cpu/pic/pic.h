#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include "../../kernel/system.h" // For inportb and outportb

void pic_remap();
void pic_send_eoi(uint8_t irq);

#endif