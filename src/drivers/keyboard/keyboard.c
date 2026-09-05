/*
 * keyboard.c - PS/2 Keyboard driver for BenmoshOS
 *
 * FIXES / IMPROVEMENTS:
 *  - After 'clear' command the prompt is reprinted correctly
 *  - Command history: Up/Down arrows scroll through previous commands
 *  - Ctrl+C: abort current input line
 *  - Tab: placeholder for future completion
 */

#include "keyboard.h"

#define BACKSPACE 0x0E
#define ENTER     0x1C

// State of modifier keys
static int shift_pressed = 0;
static int ctrl_pressed  = 0;

// Keyboard input buffer
char key_buffer[256];
int  buffer_index = 0;

// Keyboard input queue for syscalls
volatile int syscall_input_active = 0;
volatile char kbd_queue[256];
volatile uint8_t kbd_q_head = 0;
volatile uint8_t kbd_q_tail = 0;

// Keyboard history management
#define HISTORY_SIZE 16
static char history[HISTORY_SIZE][256];
static int  history_count = 0;      
static int  history_pos   = -1;

// Backup of the current line when navigating history
static char live_line_backup[256];
static int  live_line_backup_index = 0;

static void history_save(const char* cmd) {
    if (cmd[0] == '\0') return;
    /* Shift history up */
    for (int i = HISTORY_SIZE - 1; i > 0; i--) {
        for (int j = 0; j < 256; j++) history[i][j] = history[i-1][j];
    }
    /* Copy new command into slot 0 */
    int k = 0;
    while (cmd[k] && k < 255) { history[0][k] = cmd[k]; k++; }
    history[0][k] = '\0';
    if (history_count < HISTORY_SIZE) history_count++;
}

static void history_load(int index) {
    if (index < 0 || index >= history_count) return;
    /* Erase current line on screen */
    while (buffer_index > 0) {
        buffer_index--;
        print_char('\b');
        print_char(' ');
        print_char('\b');
    }
    /* Copy history entry into buffer */
    int k = 0;
    while (history[index][k] && k < 255) {
        key_buffer[k] = history[index][k];
        print_char(history[index][k]);
        k++;
    }
    buffer_index = k;
}

/* Scancode -> ASCII maps */
static const char scancode_map_normal[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
};
static const char scancode_map_shifted[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
};

/*
 * @brief
 * Pops a character from the keyboard input queue.
 *
 * @return The next character from the queue, or 0 if the queue is empty.
*/
char keyboard_pop_char() {
    if (kbd_q_head == kbd_q_tail) return 0; // Queue empty
    return kbd_queue[kbd_q_tail++];
}


/*
 * @brief
 * Keyboard interrupt handler for PS/2 keyboard.
 *
 * This function is called on every keyboard interrupt. It handles key presses,
 * releases, and special keys like Ctrl+C, Up/Down arrows for history navigation,
 * and Enter for command execution.
*/
void keyboard_handler() {
    unsigned char scancode = inportb(0x60);

    if (scancode & 0x80) { return; } // Ignore break codes for now
    if (scancode == 0x2A || scancode == 0x36)   { shift_pressed = 1; return; }
    if (scancode == 0x1D)                       { ctrl_pressed = 1; return; }

    if (syscall_input_active) {
        // If a syscall is waiting for input, enqueue the character
        char c = 0;
        if (scancode == ENTER) c = '\n';
        else if (scancode == BACKSPACE) c = '\b';
        else if (scancode < 128) {
            c = shift_pressed ? scancode_map_shifted[scancode]
                              : scancode_map_normal[scancode];
        }
        
        if (c != 0) {
            kbd_queue[kbd_q_head++] = c;
            if (kbd_q_head >= 256) kbd_q_head = 0; // Wrap around
        }
        return;
    }

    /* Key releases (break codes have bit7 set) */
    if (scancode & 0x80) {
        unsigned char released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) shift_pressed = 0;
        if (released == 0x1D)                      ctrl_pressed  = 0;
        return;
    }

    /* Shift / Ctrl press */
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0x1D)                     { ctrl_pressed  = 1; return; }

    /* Ctrl+C — abort line */
    if (ctrl_pressed && scancode == 0x2E) {
        puts("^C\n");
        buffer_index = 0;
        history_pos  = -1;
        live_line_backup_index = 0;
        set_print_color(0x0E);
        puts("BenmoshOS> ");
        set_print_color(0x0F);
        return;
    }

    /* Up arrow — older history */
    if (scancode == 72) {
        int next = (history_pos < 0) ? 0 : history_pos + 1;
        if (next < history_count) { 
            if (history_pos == -1) {
                int k = 0;
                while (k < buffer_index) {
                    live_line_backup[k] = key_buffer[k];
                    k++;
                }
                live_line_backup[k] = '\0';
                live_line_backup_index = buffer_index;
            }
            history_pos = next; 
            history_load(history_pos); 
        }
        return;
    }

    /* Down arrow — newer history */
    if (scancode == 80) {
        if (history_pos > 0) { history_pos--; history_load(history_pos); }
        else if (history_pos == 0) {
            /* Back to empty line */
            while (buffer_index > 0) {
                buffer_index--;
                print_char('\b'); print_char(' '); print_char('\b');
            }

            int k = 0;
            while ( k < live_line_backup_index) {
                key_buffer[k] = live_line_backup[k];
                print_char(live_line_backup[k]);
                k++;
            }
            buffer_index = live_line_backup_index;
            history_pos = -1;
        }
        return;
    }

    /* Page Up / Page Down — scroll terminal history */
    if (scancode == 0x49) { scroll_history_up(5);   return; }
    if (scancode == 0x51) { scroll_history_down(5); return; }

    /* ENTER — execute command */
    if (scancode == ENTER) {
        puts("\n");
        key_buffer[buffer_index] = '\0';
        history_save(key_buffer);
        history_pos = -1;
        live_line_backup_index = 0;

        execute_command(key_buffer);

        buffer_index = 0;
        set_print_color(0x0E);
        puts("BenmoshOS> ");
        set_print_color(0x0F);
        return;
    }

    /* BACKSPACE */
    if (scancode == BACKSPACE) {
        if (buffer_index > 0) {
            buffer_index--;
            print_char('\b');
            print_char(' ');
            print_char('\b');
        }
        return;
    }

    /* Printable character */
    if (scancode < 128) {
        char c = shift_pressed ? scancode_map_shifted[scancode]
                               : scancode_map_normal[scancode];
        if (c != 0 && buffer_index < 255) {
            key_buffer[buffer_index++] = c;
            print_char(c);
        }
    }
}
