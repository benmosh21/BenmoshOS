#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "../drivers/print/print.h"
#include "../fs/fat16.h"
#include "../kernel/system.h"

void execute_command(char* command);

#endif
