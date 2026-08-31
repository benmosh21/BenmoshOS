/*
 * print.c - Buffered printing with Ring Buffer, ANSI, and printf
 */

#include "print.h"
#include "../memory/heap/heap.h"
#include <stddef.h>
#include <stdarg.h> /* Required for printf */

#define MAX_ROW 25
#define MAX_COL 80
#define VIDEO_ADDRESS 0xb8000

/* The Ring Buffer variables */
int max_history_rows = 25;
uint16_t boot_buffer[25][MAX_COL];
uint8_t boot_line_lengths[25];

uint16_t (*line_buffer)[MAX_COL] = boot_buffer;
uint8_t *line_lengths = boot_line_lengths; /* Tracks where each line ended for accurate backspace */

/* Absolute counters for the ring buffer */
uint32_t absolute_write_row = 0; 
int write_col = 0;
uint32_t view_start_row = 0; 
uint8_t current_color = 0x0F;


/* --- Hardware Cursor Control --- */

void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outportb(0x3D4, 0x0A);
    outportb(0x3D5, (inportb(0x3D5) & 0xC0) | cursor_start);
    
    outportb(0x3D4, 0x0B);
    outportb(0x3D5, (inportb(0x3D5) & 0xE0) | cursor_end);
}

void disable_cursor() {
    outportb(0x3D4, 0x0A);
    outportb(0x3D5, 0x20);
}

void update_cursor(int x, int y) {
    uint16_t pos = (y * MAX_COL) + x;
    outportb(0x3D4, 0x0E);
    outportb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    outportb(0x3D4, 0x0F);
    outportb(0x3D5, (uint8_t)(pos & 0xFF));
}


/* --- Ring Buffer Screen Sync --- */

void update_screen() {
    uint16_t* video_memory = (uint16_t*)VIDEO_ADDRESS;
    uint16_t blank_cell = (uint16_t)' ' | ((uint16_t)current_color << 8);

    for (int row = 0; row < MAX_ROW; row++) {
        uint32_t abs_row = view_start_row + row;
        
        // Calculate the oldest row we still have in memory
        uint32_t oldest_row = 0;
        if (absolute_write_row >= max_history_rows) {
            oldest_row = absolute_write_row - max_history_rows + 1;
        }

        // If the row requested is within our valid ring buffer history
        if (abs_row >= oldest_row && abs_row <= absolute_write_row) {
            int buffer_idx = abs_row % max_history_rows;
            for (int col = 0; col < MAX_COL; col++) {
                video_memory[row * MAX_COL + col] = line_buffer[buffer_idx][col];
            }
        } else {
            // Out of bounds (either lost history or future blank lines)
            for (int col = 0; col < MAX_COL; col++) {
                video_memory[row * MAX_COL + col] = blank_cell;
            }
        }
    }

    // Hardware Cursor Sync
    int screen_cursor_y = absolute_write_row - view_start_row;
    if (screen_cursor_y >= 0 && screen_cursor_y < MAX_ROW) {
        update_cursor(write_col, screen_cursor_y);
    } else {
        update_cursor(0, MAX_ROW + 1); // Hide cursor if typing is happening off-screen
    }
}

void screen_clear() {
    uint16_t blank_cell = (uint16_t)' ' | ((uint16_t)current_color << 8);
    uint32_t total_cells = max_history_rows * MAX_COL;
    memsetw((unsigned short*)line_buffer, blank_cell, total_cells);
    
    absolute_write_row = 0;
    write_col = 0;
    view_start_row = 0;
    
    for(int i = 0; i < max_history_rows; i++) {
        line_lengths[i] = 0;
    }
    
    update_screen();
}


/* --- ANSI Color Parser --- */

void apply_ansi_color(int code) {
    if (code == 0) { current_color = 0x0F; return; } // Reset to White on Black
    
    // Map basic ANSI foreground codes to VGA colors
    switch (code) {
        case 30: current_color = (current_color & 0xF0) | 0x00; break; // Black
        case 31: current_color = (current_color & 0xF0) | 0x04; break; // Red
        case 32: current_color = (current_color & 0xF0) | 0x02; break; // Green
        case 33: current_color = (current_color & 0xF0) | 0x0E; break; // Yellow
        case 34: current_color = (current_color & 0xF0) | 0x01; break; // Blue
        case 35: current_color = (current_color & 0xF0) | 0x05; break; // Magenta
        case 36: current_color = (current_color & 0xF0) | 0x03; break; // Cyan
        case 37: current_color = (current_color & 0xF0) | 0x07; break; // Gray
        
        case 90: current_color = (current_color & 0xF0) | 0x08; break; // Dark Gray
        case 91: current_color = (current_color & 0xF0) | 0x0C; break; // Light Red
        case 92: current_color = (current_color & 0xF0) | 0x0A; break; // Light Green
        case 93: current_color = (current_color & 0xF0) | 0x0E; break; // Light Yellow
        case 94: current_color = (current_color & 0xF0) | 0x09; break; // Light Blue
        case 95: current_color = (current_color & 0xF0) | 0x0D; break; // Light Magenta
        case 96: current_color = (current_color & 0xF0) | 0x0B; break; // Light Cyan
        case 97: current_color = (current_color & 0xF0) | 0x0F; break; // White
    }
}


/* --- Printing Logic --- */

void print_char(char character) {
    static int ansi_state = 0;
    static int ansi_code = 0;

    // ANSI Escape Sequence State Machine
    if (ansi_state == 0) {
        if (character == '\033') { ansi_state = 1; return; }
    } else if (ansi_state == 1) {
        if (character == '[') { ansi_state = 2; ansi_code = 0; return; }
        ansi_state = 0; 
    } else if (ansi_state == 2) {
        if (character >= '0' && character <= '9') {
            ansi_code = ansi_code * 10 + (character - '0');
            return;
        }
        if (character == 'm') {
            apply_ansi_color(ansi_code);
            ansi_state = 0;
            return;
        }
        ansi_state = 0; 
    }

    // Normal Character Processing
    uint32_t current_buffer_idx = absolute_write_row % max_history_rows;
    uint16_t blank_cell = (uint16_t)' ' | ((uint16_t)current_color << 8);

    if (character == '\n') {
        line_lengths[current_buffer_idx] = write_col; // Save line length
        write_col = 0;
        absolute_write_row++;
        
        // Wipe the newly acquired row in the ring buffer ahead of us
        memsetw(line_buffer[absolute_write_row % max_history_rows], blank_cell, MAX_COL);

    } else if (character == '\b') {
        if (write_col > 0) {
            write_col--;
            line_buffer[current_buffer_idx][write_col] = blank_cell;
        } else if (absolute_write_row > 0) {
            // Jump back to the end of the previous line using our tracking array
            absolute_write_row--;
            current_buffer_idx = absolute_write_row % max_history_rows;
            write_col = line_lengths[current_buffer_idx];
            
            // Safety bound check
            if (write_col >= MAX_COL) write_col = MAX_COL - 1;
            line_buffer[current_buffer_idx][write_col] = blank_cell;
        }

    } else {
        line_buffer[current_buffer_idx][write_col] = (uint16_t)((uint8_t)character) | ((uint16_t)current_color << 8);
        write_col++;

        if (write_col >= MAX_COL) {
            line_lengths[current_buffer_idx] = MAX_COL;
            write_col = 0;
            absolute_write_row++;
            memsetw(line_buffer[absolute_write_row % max_history_rows], blank_cell, MAX_COL);
        }
    }

    // Auto-scroll screen lock
    if (absolute_write_row >= view_start_row + MAX_ROW) {
        view_start_row = absolute_write_row - MAX_ROW + 1;
    }

    update_screen();
}

void puts(char *str) {
    int i = 0;
    while (str[i] != 0) {
        print_char(str[i]);
        i++;
    }
}


/* --- Scrolling Logic --- */

void scroll_history_up(int lines) {
    if (view_start_row >= (uint32_t)lines) {
        view_start_row -= lines;
    } else {
        view_start_row = 0;
    }
    
    // Prevent scrolling into forgotten history
    uint32_t oldest_row = 0;
    if (absolute_write_row >= max_history_rows) {
        oldest_row = absolute_write_row - max_history_rows + 1;
    }
    if (view_start_row < oldest_row) {
        view_start_row = oldest_row;
    }
    
    update_screen();
}

void scroll_history_down(int lines) {
    uint32_t max_view_start = 0;
    if (absolute_write_row >= MAX_ROW - 1) {
        max_view_start = absolute_write_row - MAX_ROW + 1;
    }

    view_start_row += lines;
    if (view_start_row > max_view_start) {
        view_start_row = max_view_start;
    }
    
    update_screen();
}



void print_int(int n) {
    char buffer[12];
    itoa(n, buffer);
    puts(buffer);
}

void print_hex(int n) {
    char buffer[9];
    for (int i = 0; i < 8; i++) {
        int digit = (n >> ((7 - i) * 4)) & 0xF;
        if (digit < 10) {
            buffer[i] = '0' + digit;
        } else {
            buffer[i] = 'A' + (digit - 10);
        }
    }
    buffer[8] = '\0';
    
    // Trim leading zeros for a cleaner look
    int start = 0;
    while (buffer[start] == '0' && start < 7) start++;
    
    puts("0x");
    puts(&buffer[start]);
}

void set_print_color(uint16_t color) {
    current_color = color;
}

void enable_dynamic_history(int total_rows) {
    // Allocate the large ring buffer
    uint16_t(*new_buffer)[MAX_COL] = (uint16_t(*)[MAX_COL])malloc(total_rows * MAX_COL * sizeof(uint16_t));
    
    // Allocate the parallel line length tracking array
    uint8_t *new_lengths = (uint8_t*)malloc(total_rows * sizeof(uint8_t));

    if (new_buffer != NULL && new_lengths != NULL) {
        line_buffer = new_buffer;
        line_lengths = new_lengths;
        max_history_rows = total_rows;
        
        // Reset state for the new memory location
        screen_clear();
    }
}

void print_scancode(uint8_t scancode) {
    puts("SC:");
    print_hex(scancode);
    puts(" ");
}