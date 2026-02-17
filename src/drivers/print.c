/*
 * print.c - Print functions for our OS
 * This file contains functions to print strings to the screen.
 */

#include "print.h"

void print(char *str, char *screen) {
    for (int i = 0; str[i] != 0; i++) {
       
        *screen = str[i];
        
        *(screen + 1) = 0x0f; 
        
        screen += 2;
    }
}