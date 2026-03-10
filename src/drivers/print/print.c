/*
 * print.c - Buffered printing with history
 */

#include "print.h"
#include "../memory/heap/heap.h"

#define MAX_ROW 25
#define MAX_COL 80
#define BUFFER_ROWS 4000  // Size of our history
#define VIDEO_ADDRESS 0xb8000



// The Big Buffer: Stores all text history
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
    // Treat the screen as an array of 16-bit blocks, not 8-bit chars
    uint16_t* video_memory = (uint16_t*)VIDEO_ADDRESS;

    for (int row = 0; row < MAX_ROW; row++) {
        int buffer_index = view_start_row + row;

        for (int col = 0; col < MAX_COL; col++) {
            // Default to a colored space
            uint16_t cell = (uint16_t)' ' | ((uint16_t)current_color << 8);

            // Safety check: Only read if we are inside the buffer
            if (buffer_index >= 0 && buffer_index < BUFFER_ROWS) {
                if (line_buffer[buffer_index][col] != 0) {
                    cell = line_buffer[buffer_index][col];
                }
            }

            // Write the entire 16-bit block directly to the VGA hardware
            video_memory[row * MAX_COL + col] = cell;
        }
    }

    // --- Sync the Hardware Cursor ---
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
    else if (character == '\b') {
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
        if (write_row < BUFFER_ROWS) {
            // STRICT CASTING: Prevents C from corrupting the color bits
            line_buffer[write_row][write_col] = (uint16_t)((uint8_t)character) | ((uint16_t)current_color << 8);
        }
        write_col++;

        if (write_col >= MAX_COL) {
            write_col = 0;
            write_row++;
        }
    }

    // --- Scrolling Logic ---
    if (write_row >= BUFFER_ROWS) {
        for (int r = 1; r < BUFFER_ROWS; r++) {
            for (int col = 0; col < MAX_COL; col++) {
                line_buffer[r - 1][col] = line_buffer[r][col];
            }
        }

        // Clear the bottom row with properly colored spaces
        for (int col = 0; col < MAX_COL; col++) {
            line_buffer[BUFFER_ROWS - 1][col] = (uint16_t)' ' | ((uint16_t)current_color << 8);
        }
        write_row = BUFFER_ROWS - 1;
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