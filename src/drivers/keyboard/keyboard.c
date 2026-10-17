#include "keyboard.h"

#define BACKSPACE 0x0E
#define ENTER     0x1C

// State of modifier keys
static int shift_pressed = 0;
static int ctrl_pressed  = 0;

#define KBD_BUFFER_SIZE 128
static char kbd_fifo[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;


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

char keyboard_pop_char() {
    if (kbd_head == kbd_tail) return 0; // Empty
    char c = kbd_fifo[kbd_tail];
    kbd_tail = (kbd_tail+1) % KBD_BUFFER_SIZE;
    return c;
}

void keyboard_handler() {
    uint8_t scancode = inportb(0x60);
    
    // key releases (break codes)
    if (scancode & 0x80) {
        unsigned char key_released = scancode & 0x7F;
        if (key_released == 0x2A || key_released == 0x36) {shift_pressed = 0; return; }
        if (key_released == 0x1D) {ctrl_pressed = 0; return;}
        return;
    }

    // Presses (make codes)
    if (scancode == 0x2A || scancode == 0x36) {shift_pressed = 1; return; }
    if (scancode == 0x1D) {ctrl_pressed = 1; return; }
    
    // Page Up / Page Down
    if (scancode == 0x49) { scroll_history_up(5); return; }
    if (scancode == 0x51) { scroll_history_down(5); return; }


    // Convert scancode to ASCII / special toket
    char c = 0;
    if (scancode == ENTER) c = '\n';
    else if (scancode == BACKSPACE) {
         c = '\b';
    } else if (ctrl_pressed && scancode == 0x2E) { // Ctrl+C
        c = 3; // ASCII ETX (End of Text)
    } else if (scancode == 72) {
        c = 0x11; // Up Arrow
    } else if (scancode == 80) {
        c = 0x12; // Down Arrow
    } else if (scancode < 128) {
        c = shift_pressed ? scancode_map_shifted[scancode]
                          : scancode_map_normal[scancode];
    }

    if (c != 0) {
        int next = (kbd_head + 1) % KBD_BUFFER_SIZE;
        if (next != kbd_tail) { // Nut full
            kbd_fifo[kbd_head] = c;
            kbd_head = next;
        }
    }
}


// let programs read a character
char kbd_getchar(void) {
    if (kbd_head == kbd_tail) return 0; // Empty
    char c = kbd_fifo[kbd_tail];
    kbd_tail = (kbd_tail+1) % KBD_BUFFER_SIZE;
    return c;
}
