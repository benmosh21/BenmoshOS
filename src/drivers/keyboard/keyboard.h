#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "../print/print.h"
#include "../../kernel/system.h"

/* Forward declaration — avoids circular include with shell.h */
void execute_command(char* command);

void keyboard_handler();

extern volatile int syscall_input_active;
char keyboard_pop_char();

#endif
