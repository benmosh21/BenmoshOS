#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include "../../kernel/system.h"


// Print functions for our OS
void print_char(char c);
void print(char *str);
void print_int(int n);
void print_hex(int n);
void print_float(float f);
void print_scancode(uint8_t scancode);
void screen_clear();
void update_screen();
// Scrolling functions
void scroll_history_up(int lines);
void scroll_history_down(int lines);
// Helper functions
void reverse(char s[]);
void itoa(int n, char str[]);

// Color definitions
typedef enum {
	WHITE_ON_BLACK = 0x0F,
	RED_ON_BLACK = 0x0C,
	GREEN_ON_BLACK = 0x0A,
	YELLOW_ON_BLACK = 0x0E,
	BLUE_ON_BLACK = 0x09,
	MAGENTA_ON_BLACK = 0x0D,
	CYAN_ON_BLACK = 0x0B,

	BLACK_ON_WHITE = 0xF0,
	RED_ON_WHITE = 0xC0,
	GREEN_ON_WHITE = 0xA0,
	YELLOW_ON_WHITE = 0xE0,
	BLUE_ON_WHITE = 0x90,
	MAGENTA_ON_WHITE = 0xD0,
	CYAN_ON_WHITE = 0xB0,

	BLACK_ON_RED = 0x40,
	WHITE_ON_RED = 0x47,
	GREEN_ON_RED = 0x42,
	YELLOW_ON_RED = 0x46,
	BLUE_ON_RED = 0x41,
	MAGENTA_ON_RED = 0x44,
	CYAN_ON_RED = 0x43,

	BLACK_ON_GREEN = 0x20,
	WHITE_ON_GREEN = 0x27,
	RED_ON_GREEN = 0x24,
	YELLOW_ON_GREEN = 0x26,
	BLUE_ON_GREEN = 0x21,
	MAGENTA_ON_GREEN = 0x24,
	CYAN_ON_GREEN = 0x23,

	BLACK_ON_YELLOW = 0x60,
	WHITE_ON_YELLOW = 0x67,
	RED_ON_YELLOW = 0x64,
	GREEN_ON_YELLOW = 0x62,
	BLUE_ON_YELLOW = 0x61,
	MAGENTA_ON_YELLOW = 0x64,
	CYAN_ON_YELLOW = 0x63,

	BLACK_ON_BLUE = 0x10,
	WHITE_ON_BLUE = 0x17,
	RED_ON_BLUE = 0x14,
	GREEN_ON_BLUE = 0x12,
	YELLOW_ON_BLUE = 0x16,
	MAGENTA_ON_BLUE = 0x14,
	CYAN_ON_BLUE = 0x13,

	BLACK_ON_MAGENTA = 0x50,
	WHITE_ON_MAGENTA = 0x57,
	RED_ON_MAGENTA = 0x54,
	GREEN_ON_MAGENTA = 0x52,
	YELLOW_ON_MAGENTA = 0x56,
	BLUE_ON_MAGENTA = 0x51,
	CYAN_ON_MAGENTA = 0x53,

	BLACK_ON_CYAN = 0x30,
	WHITE_ON_CYAN = 0x37,
	RED_ON_CYAN = 0x34,
	GREEN_ON_CYAN = 0x32,
	YELLOW_ON_CYAN = 0x36,
	BLUE_ON_CYAN = 0x31,
	MAGENTA_ON_CYAN = 0x34
} Color ;

void set_print_color(uint16_t color);
void enable_dynamic_history(int total_rows);

#endif