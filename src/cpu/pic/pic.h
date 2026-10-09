#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include "../ports/ports.h"

void pic_remap();
void pic_send_eoi(uint8_t irq);

void init_timer(uint32_t frequency);

#endif