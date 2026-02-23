#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "../print/print.h"
#include "../../kernel/system.h" // For inportb
#include "../../shell/shell.h"

void keyboard_handler();

#endif