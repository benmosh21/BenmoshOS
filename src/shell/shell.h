#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "../drivers/print/print.h"
#include "../fs/fat16.h"
#include "../kernel/system.h"
#include "../memory/heap/heap.h"
#include "../libc/stdio.h"
#include "../libc/strings.h"
#include "../libc/stdlib.h"

void execute_command(char* command);

#endif
