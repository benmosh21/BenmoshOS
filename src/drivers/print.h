#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include "../kernel/system.h"


// Print functions for our OS
void print_char(char c);
void print(char *str);
void print_int(int n);
void print_scancode(uint8_t scancode);
void screen_clear();
void update_screen();
// Scrolling functions
void scroll_history_up(int lines);
void scroll_history_down(int lines);
// Helper functions
void reverse(char s[]);
void itoa(int n, char str[]);

#endif