#include "stdio.h"
#include <stdarg.h>
#include <stddef.h>
#include "../drivers/print/print.h"

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    int i = 0;
    while (format[i] != '\0') {
        if (format[i] == '%') {
            i++;
            if (format[i] == '\0') {
                break; // Handle case where '%' is at the end of the string
            }
            switch (format[i]) {
            case 'd':
            case 'i':
                print_int(va_arg(args, int));
                break;
            case 'x':
            case 'X':
                print_hex(va_arg(args, int));
                break;
            case 's': {
                char* str = va_arg(args, char*);
                if (str == NULL) {
                    puts("(null)");
                }
                else {
                    puts(str);
                }
                break;
            }
            case 'c':
                print_char((char)va_arg(args, int));
                break;
            case '%':
                print_char('%');
                break;
            default:
                print_char('%');
                print_char(format[i]);
                break;
            }
        }
        else {
            print_char(format[i]);
        }
        i++;
    }

    va_end(args);
}