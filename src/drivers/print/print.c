/*
 * print.c - Buffered printing with history
 */

#include "print.h"
#include "../memory/heap/heap.h"
#include <stddef.h>

#define MAX_ROW 25
#define MAX_COL 80
#define BUFFER_ROWS 4000  // Size of our history
#define VIDEO_ADDRESS 0xb8000



// The Big Buffer: Stores all text history
int max_history_rows = 25; // Stage 1: Tiny safe limit for early boot
uint16_t boot_buffer[25][MAX_COL]; // Stage 1: The tiny array that fits in the bootloader
uint16_t(*line_buffer)[MAX_COL];

// Cursors
int write_row = 0;     // Current line we are writing to in the buffer
int write_col = 0;     // Current column in that line
int view_start_row = 0;// The top line currently visible on screen
uint8_t current_color = 0x0F; // Default color

void update_cursor(int x, int y) {
    uint16_t pos = (y * MAX_COL) +x;

    // Send the High byte of the position
    outportb(0x3D4, 0x0E);
    outportb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    
    // Send the Low byte of the position
    outportb(0x3D4, 0x0F);
    outportb(0x3D5, (uint8_t)(pos & 0xFF));
}

// --- Core Helper: Sync Buffer to Screen ---
// This copies MAX_ROWS(25) lines from the buffer to the actual video memory
void update_screen() {
    // Cast the VGA hardware directly to a 16-bit pointer
    uint16_t* video_memory = (uint16_t*)VIDEO_ADDRESS;

    for (int row = 0; row < MAX_ROW; row++) {
        int buffer_index = view_start_row + row;

        for (int col = 0; col < MAX_COL; col++) {
            // Default: A blank space merged with the current color
            uint16_t cell = (uint16_t)' ' | ((uint16_t)current_color << 8);

            // Safety check: Only read if we are inside our known history bounds
            if (buffer_index >= 0 && buffer_index < max_history_rows) {
                if (line_buffer[buffer_index][col] != 0) {
                    cell = line_buffer[buffer_index][col];
                }
            }

            // Write the 16-bit block directly to the VGA hardware
            video_memory[row * MAX_COL + col] = cell;
        }
    }

    // Hardware Cursor Sync
    int screen_cursor_y = write_row - view_start_row;
    if (screen_cursor_y >= 0 && screen_cursor_y < MAX_ROW) {
        update_cursor(write_col, screen_cursor_y);
    }
    else {
        update_cursor(0, MAX_ROW + 1);
    }
}

void screen_clear() {

	line_buffer = (uint16_t(*)[MAX_COL])malloc(BUFFER_ROWS * MAX_COL * sizeof(uint16_t));

    for (int i = 0; i < BUFFER_ROWS; i++) {
        for (int j = 0; j < MAX_COL; j++) {
            line_buffer[i][j] = (uint16_t)' ' | ((uint16_t)current_color << 8);
        }
    }
    write_row = 0;
    write_col = 0;
    view_start_row = 0;
    update_screen();
}

// --- Printing Logic ---

void print_char(char character) {
    if (character == '\n') {
        write_col = 0;
        write_row++;
    }
    else if (character == '\b') { // Backspace
        if (write_col > 0) {
            write_col--;
            line_buffer[write_row][write_col] = (uint16_t)' ' | ((uint16_t)current_color << 8);
        }
        else if (write_row > 0) {
            write_row--;
            write_col = MAX_COL - 1;
            line_buffer[write_row][write_col] = (uint16_t)' ' | ((uint16_t)current_color << 8);
        }
    }
    else {
        if (write_row < max_history_rows) {
            // THE MAGIC MATH: Combine the letter and the color!
            line_buffer[write_row][write_col] = (uint16_t)((uint8_t)character) | ((uint16_t)current_color << 8);
        }
        write_col++;

        if (write_col >= MAX_COL) {
            write_col = 0;
            write_row++;
        }
    }

    // Scrolling Logic
    if (write_row >= max_history_rows) {
        for (int r = 1; r < max_history_rows; r++) {
            for (int col = 0; col < MAX_COL; col++) {
                line_buffer[r - 1][col] = line_buffer[r][col];
            }
        }
        for (int col = 0; col < MAX_COL; col++) {
            line_buffer[max_history_rows - 1][col] = (uint16_t)' ' | ((uint16_t)current_color << 8);
        }
        write_row = max_history_rows - 1;
    }

    if (write_row >= view_start_row + MAX_ROW) {
        view_start_row = write_row - MAX_ROW + 1;
    }

    update_screen();
}

void print(char *str) {
    int i = 0;
    while (str[i] != 0) {
        print_char(str[i]);
        i++;
    }
}

// --- Scrolling Logic (Just moving the camera!) ---

void scroll_history_up(int lines) {
    if (view_start_row > 0) {
        view_start_row -= lines;
        if (view_start_row < 0) view_start_row = 0;
        update_screen();
    }
}

void scroll_history_down(int lines) {
    int max_view_start = write_row - MAX_ROW + 1;

    if (max_view_start < 0) {
        max_view_start = 0;
    }

    // Don't scroll past the text we have written
    if (view_start_row < max_view_start) {
        view_start_row += lines; // Look "down" (higher index)
        if (view_start_row > max_view_start) {
            view_start_row = max_view_start;
        }
        update_screen();
    }
}

// --- Helpers (itoa / reverse) ---
// (Keep your existing reverse and itoa functions here, they were perfect!)

void reverse(char s[]) {
    int i, j;
    char c;
    for (i = 0, j = 0; s[j] != 0; j++);
    j--;
    for (; i < j; i++, j--) {
        c = s[i]; s[i] = s[j]; s[j] = c;
    }
}

void itoa(int n, char str[]) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';
    reverse(str);
}

void print_int(int n) {
    char buffer[12];
    itoa(n, buffer);
    print(buffer);
}

void print_hex(int n) {
	char buffer[9];
	for (int i = 0; i < 8; i++) {
		int digit = (n >> ((7 - i) * 4)) & 0xF;
		if (digit < 10) {
			buffer[i] = '0' + digit;
		}
		else {
			buffer[i] = 'A' + (digit - 10);
		}
	}
	buffer[8] = '\0';
	print(buffer);
}

void print_float(float f) {
    int int_part = (int)f;
    float frac_part = f - int_part;

    print_int(int_part);
    print(".");

    // Print 6 decimal places
    for (int i = 0; i < 6; i++) {
        frac_part *= 10;
        int digit = (int)frac_part;
        print_int(digit);
        frac_part -= digit;
    }
}

void set_color(uint16_t color) {
	current_color = color;
}

void enable_dynamic_history(int total_rows) {
    // Stage 2: Ask the heap for a massive chunk of RAM
    uint16_t(*new_buffer)[MAX_COL] = (uint16_t(*)[MAX_COL])malloc(total_rows * MAX_COL * sizeof(uint16_t));

    if (new_buffer != NULL) {
        // Swap the master pointer to the massive heap memory
        line_buffer = new_buffer;
        max_history_rows = total_rows;

        // Wipe the new memory clean so we don't print random garbage
        screen_clear();
    }
    // If malloc fails, we silently keep using the safe boot_buffer
}