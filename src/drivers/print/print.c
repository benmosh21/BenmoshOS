/*
 * print.c - Buffered printing with history
 */

#include "print.h"

#define MAX_ROW 25
#define MAX_COL 80
#define BUFFER_ROWS 4000  // Size of our history
#define VIDEO_ADDRESS 0xb8000
#define WHITE_ON_BLACK 0x0f

// The Big Buffer: Stores all text history
char line_buffer[BUFFER_ROWS][MAX_COL]; 

// Cursors
int write_row = 0;     // Current line we are writing to in the buffer
int write_col = 0;     // Current column in that line
int view_start_row = 0;// The top line currently visible on screen

void update_cursor(int x, int y) {
    uint16_t pos = (y * MAX_COL) +x;

    // Send the High byte of the position
    outportb(0x3D4, 0x0E);
    outportb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    
    // Send the Low byte of the position
    outportb(0x3D4, 0x0F);
    outportb(0x3D5, (uint8_t)(pos & 0xFF));}

// --- Core Helper: Sync Buffer to Screen ---
// This copies MAX_ROWS(25) lines from the buffer to the actual video memory
void update_screen() {
    char *video_memory = (char*) VIDEO_ADDRESS;
    
    for (int row = 0; row < MAX_ROW; row++) {
        int buffer_index = view_start_row + row;

        for (int col = 0; col < MAX_COL; col++) {
            // Calculate video memory offset
            int offset = 2 * (row * MAX_COL + col);
            
            // Get char from buffer (safely)
            char c = ' ';
            if (buffer_index < BUFFER_ROWS) {
                c = line_buffer[buffer_index][col];
            }
            if (c == 0) c = ' '; // Convert nulls to spaces for display

            video_memory[offset] = c;
            video_memory[offset + 1] = WHITE_ON_BLACK;
        }
    }

    // --- Sync the Hardware Cursor ---
    // Calculate the cursor's position relative to the camera view
    int screen_cursor_y = write_row - view_start_row;
    
    // Only draw the cursor if it is currently visible on the screen
    if (screen_cursor_y >= 0 && screen_cursor_y < MAX_ROW) {
        update_cursor(write_col, screen_cursor_y);
    } else {
        // If we scrolled away from the cursor, hide it off-screen
        update_cursor(0, MAX_ROW + 1); 
    }
}

void screen_clear() {
    // Clear the buffer
    for (int i = 0; i < BUFFER_ROWS; i++) {
        for (int j = 0; j < MAX_COL; j++) {
            line_buffer[i][j] = ' ';
        }
    }
    write_row = 0;
    write_col = 0;
    view_start_row = 0;
    update_screen();
}

// --- Printing Logic ---

void print_char(char c) {
    if (c == '\n') {
        write_col = 0;
        write_row++;
    } else if (c == '\b') {
        if (write_col > 0) {
            write_col--;
            line_buffer[write_row][write_col] = ' ';
        } else if (write_row > 0) {
            // Jump back to the previous line.
            write_row--;
            write_col = MAX_COL - 1;
            line_buffer[write_col][write_row]  = ' ';
        } 
    } else {
        // Safety: Don't write past buffer end
        if (write_row < BUFFER_ROWS) {
            line_buffer[write_row][write_col] = c;
        }
        write_col++;

        // Wrap to next line if we hit edge
        if (write_col >= MAX_COL) {
            write_col = 0;
            write_row++;
        }
    }

    if (write_row >= BUFFER_ROWS) {
        // Shift every row up by one
        for (int r = 1; r < BUFFER_ROWS; r++) {
            for (int c = 0; c < MAX_COL; c++) {
                line_buffer[r-1][c] = line_buffer[r][c];
            }
        }
        
        // Clear the last row (now row 99) so it's ready for new text
        for (int c = 0; c < MAX_COL; c++) {
            line_buffer[BUFFER_ROWS-1][c] = ' ';
        }

        // Keep our write pointer at the last line
        write_row = BUFFER_ROWS - 1;
    }

    // --- Auto-Scroll Logic ---
    // Ensure the view follows the cursor
    if (write_row >= view_start_row + MAX_ROW) {
        view_start_row = write_row - MAX_ROW + 1;
    }
    
    // Update the screen
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

