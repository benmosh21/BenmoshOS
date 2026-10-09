#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>
#include "../drivers/print/print.h"


void printf(const char* format, ...);
void vprintf(const char* format, va_list args);

#endif