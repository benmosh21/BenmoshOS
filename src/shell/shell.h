#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "../drivers/print/print.h"
#include "../fs/fat16.h"
#include "../cpu/ports/ports.h"
#include "../memory/heap/heap.h"
#include "../memory/pmm/pmm.h"
#include "../klib/stdio.h"
#include "../klib/string.h"
#include "../klib/stdlib.h"

void execute_command(char* command);

#endif
