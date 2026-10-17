#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "../print/print.h"
#include "../../cpu/ports/ports.h"

void keyboard_handler(void);
char keyboard_pop_char(void);
char kbd_getchar(void);

#endif
