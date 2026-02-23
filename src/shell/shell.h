#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/print/print.h"
#include "../fs/fat16.h"

void execute_command(char* command);

#endif
