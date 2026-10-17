#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/print/print.h"
#include "../kernel/task/task.h"
#include "../cpu/ports/ports.h"
#include "../memory/heap/heap.h"
#include "../memory/pmm/pmm.h"
#include "../klib/string.h"
#include "../klib/stdlib.h"
#include "../klib/stdio.h"
#include "../fs/fat16.h"

void execute_command(char* command);
void shell_run(void);
void shell_keyboard(void);

#endif
