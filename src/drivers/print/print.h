#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include "../../kernel/system.h"

/* Forward declarations from system.h to break circular include */
void outportb(uint16_t _port, unsigned char _data);

/* Core print functions */
void print_char(char c);
void puts(const char *str);
void print_int(int n);
void print_hex(int n);
//void print_float(float f); - removed untile i implement FPU
void print_scancode(uint8_t scancode);

/* Screen control */
void screen_clear();
void update_screen();
void scroll_history_up(int lines);
void scroll_history_down(int lines);
void set_print_color(uint16_t color);
void enable_dynamic_history(int total_rows);

/* hardware functions */
void enable_cursor(uint8_t cursor_start, uint8_t cursor_end);
void disable_cursor();
void update_cursor(int x, int y);

/* Color constants */
typedef enum {
    WHITE_ON_BLACK    = 0x0F,
    RED_ON_BLACK      = 0x0C,
    GREEN_ON_BLACK    = 0x0A,
    YELLOW_ON_BLACK   = 0x0E,
    BLUE_ON_BLACK     = 0x09,
    MAGENTA_ON_BLACK  = 0x0D,
    CYAN_ON_BLACK     = 0x0B,
    GRAY_ON_BLACK     = 0x07,
    BLACK_ON_WHITE    = 0xF0,
    WHITE_ON_BLUE     = 0x17,
    WHITE_ON_RED      = 0x47,
    WHITE_ON_GREEN    = 0x27,
} Color;

#endif
