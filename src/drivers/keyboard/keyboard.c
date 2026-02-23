#include "keyboard.h"

#define BACKSPACE 0x0E
#define ENTER 0x1C

// --- State and Buffer ---
static int shift_pressed = 0;

// The buffer where we store the user's current command
char key_buffer[256];
int buffer_index = 0;

// (Copy your scancode_map_normal and scancode_map_shifted arrays here)
static const char scancode_map_normal[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };
static const char scancode_map_shifted[128] = { 0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };

// --- The Main Handler ---
void keyboard_handler() {
    unsigned char scancode = inportb(0x60);

    // 1. Check for Key Releases (Break Codes)
    if (scancode & 0x80) {
        if (scancode == 0xAA || scancode == 0xB6) shift_pressed = 0;
        return;
    }

    // 2. Check for Shift
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }

    // 3. Handle Special Action Keys (Up/Down)
    if (scancode == 72) { scroll_history_up(1); return; } 
    if (scancode == 80) { scroll_history_down(1); return; }

    // 4. Handle ENTER key (Execute command)
    if (scancode == ENTER) {
        print("\n");
        key_buffer[buffer_index] = '\0'; // Null-terminate the string
        
		// Execute the command in the buffer
		execute_command(key_buffer);
        print("BenmoshOS> ");

        // Reset the buffer for the next command
        buffer_index = 0;
        return;
    }

    if (scancode == BACKSPACE) {
        if (buffer_index > 0) {
            buffer_index--;
            print_char('\b'); // Move cursor back
            print_char(' ');  // Erase the character
            print_char('\b'); // Move cursor back again
        }
        return;
    }

    // 5. Handle Printable Characters
    if (scancode < 128) {
        char c = shift_pressed ? scancode_map_shifted[scancode] : scancode_map_normal[scancode];

        if (c != 0) {
            // Add to buffer and print to screen
            if (buffer_index < 255) {
                key_buffer[buffer_index] = c;
                buffer_index++;
                print_char(c);
            }
        }
    }
}