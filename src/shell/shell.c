#include "shell.h"

#define MAX_HISTORY     256
#define MAX_LINE_LENGTH 256
#define MAX_ARGS        32

/* Ring/array buffer storing previous command strings */
static char history[MAX_HISTORY][MAX_LINE_LENGTH];
static int history_count = 0;

/* Current navigation position in history: -1 indicates active live typing */
static int history_pos = -1;

/* Temporary snapshot of the current line before the user browsed history */
static char live_line_backup[MAX_LINE_LENGTH];

/* Active input buffer typed by the user */
static char key_buffer[MAX_LINE_LENGTH];
static int buffer_idx = 0;

void execute_command(char* command);

/**
 * Saves a completed command line into the history array.
 * Shifts previous commands down and inserts the new command at index 0.
 */
static void history_save(const char* cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        return;
    }

    /* Avoid storing duplicate consecutive commands */
    if (history_count > 0 && strcmp(history[0], cmd) == 0) {
        return;
    }

    /* Shift older entries down to make room at index 0 */
    for (int i = MAX_HISTORY - 1; i > 0; i--) {
        strcpy(history[i], history[i - 1]);
    }

    /* Store the new command safely */
    strncpy(history[0], cmd, MAX_LINE_LENGTH - 1);
    history[0][MAX_LINE_LENGTH - 1] = '\0';

    if (history_count < MAX_HISTORY) {
        history_count++;
    }
}

/**
 * Visually clears the current command line on screen, updates key_buffer
 * with new_line, and prints it out. Ensures buffer_idx and string termination
 * remain synchronized.
 */
static void shell_replace_line(const char* new_line) {
    /* Erase all visible characters currently on screen */
    while (buffer_idx > 0) {
        print_char('\b');
        print_char(' ');
        print_char('\b');
        buffer_idx--;
    }

    /* Copy the replacement string into the live buffer and print each character */
    int k = 0;
    while (new_line[k] != '\0' && k < MAX_LINE_LENGTH - 1) {
        key_buffer[k] = new_line[k];
        print_char(key_buffer[k]);
        k++;
    }

    key_buffer[k] = '\0';
    buffer_idx = k;
}

/**
 * Loads a command from history by index and displays it.
 */
static void history_load(int pos) {
    if (pos < 0 || pos >= history_count) {
        return;
    }
    shell_replace_line(history[pos]);
}

/**
 * Halts CPU execution completely after displaying a shutdown message.
 */
static void shell_exit(void) {
    puts("\nShutting down BenmoshOS... Goodbye!\n");

    /* Disable interrupts and halt indefinitely in a loop against NMIs */
    while (1) {
        __asm__ __volatile__(
            "cli\n\t"
            "hlt"
        );
    }
}

/**
 * Processes a single keyboard event, updates screen state, and executes
 * commands when Enter is pressed.
 */
void shell_keyboard(void) {
    char c = kbd_getchar();
    if (c == 0) {
        /* No key available: surrender remaining time-slice */
        task_yield();
        return;
    }

    /* Ctrl+C: Cancel current command line and print a fresh prompt */
    if (c == 0x03) {
        puts("^C\n");
        buffer_idx = 0;
        key_buffer[0] = '\0';
        live_line_backup[0] = '\0';
        history_pos = -1;

        set_print_color(0x0E);
        puts("BenmoshOS> ");
        set_print_color(0x0F);
        return;
    }

    /* Up Arrow (0x11): Browse older history entries */
    if (c == 0x11) {
        if (history_count == 0) {
            return;
        }

        int next = (history_pos < 0) ? 0 : history_pos + 1;
        if (next < history_count) {
            /* If transitioning from the live line, back it up first */
            if (history_pos < 0) {
                key_buffer[buffer_idx] = '\0';
                strcpy(live_line_backup, key_buffer);
            }
            history_pos = next;
            history_load(history_pos);
        }
        return;
    }

    /* Down Arrow (0x12): Browse newer history entries or restore live line */
    if (c == 0x12) {
        if (history_pos > 0) {
            history_pos--;
            history_load(history_pos);
        } else if (history_pos == 0) {
            /* Returning past the newest history entry restores the live line */
            history_pos = -1;
            shell_replace_line(live_line_backup);
        }
        return;
    }

    /* Enter: Submit the line for command parsing */
    if (c == '\n') {
        print_char('\n');
        key_buffer[buffer_idx] = '\0';

        /* Save the actual command line that was just entered */
        history_save(key_buffer);

        /* Create a local copy to execute since key_buffer is reset */
        char cmd_copy[MAX_LINE_LENGTH];
        strcpy(cmd_copy, key_buffer);

        /* Reset buffer state for the next command */
        buffer_idx = 0;
        key_buffer[0] = '\0';
        live_line_backup[0] = '\0';
        history_pos = -1;

        execute_command(cmd_copy);

        set_print_color(0x0E);
        puts("BenmoshOS> ");
        set_print_color(0x0F);
        return;
    }

    /* Backspace: Erase previous character if buffer is non-empty */
    if (c == '\b') {
        if (buffer_idx > 0) {
            buffer_idx--;
            key_buffer[buffer_idx] = '\0';
            print_char('\b');
            print_char(' ');
            print_char('\b');
        }
        return;
    }

    /* Printable characters: Append to key_buffer and echo to display */
    if (buffer_idx < MAX_LINE_LENGTH - 1) {
        key_buffer[buffer_idx++] = c;
        key_buffer[buffer_idx] = '\0';
        print_char(c);
    }
}

/**
 * Main shell loop. Prints initial prompt and polls for input.
 */
void shell_run(void) {
    set_print_color(0x0E);
    puts("BenmoshOS> ");
    set_print_color(0x0F);

    while (1) {
        shell_keyboard();
    }
}

/**
 * Tokenizes the input string into whitespace-separated arguments and
 * dispatches the corresponding built-in command.
 */
void execute_command(char* command) {
    if (command == NULL) {
        return;
    }

    /* Allocate argv on the stack to prevent kernel heap fragmentation and leaks */
    char* argv[MAX_ARGS];
    int argc = 0;
    int in_word = 0;

    /* Tokenize in-place by replacing whitespace characters with null bytes */
    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] == ' ' || command[i] == '\t' || command[i] == '\n' || command[i] == '\r') {
            command[i] = '\0';
            in_word = 0;
        } else if (in_word == 0) {
            if (argc >= MAX_ARGS - 1) {
                puts("shell: too many arguments\n");
                return;
            }
            in_word = 1;
            argv[argc++] = &command[i];
        }
    }
    argv[argc] = NULL;

    /* Empty input (e.g. user just hit Enter with spaces) */
    if (argc == 0) {
        return;
    }

    if (!strcmp(argv[0], "ls")) {
        if (argc > 1) {
            puts("Usage: ls (no arguments)\n");
        } else {
            puts("Files on disk:\n");
            fat16_list_files();
        }
    }
    else if (!strcmp(argv[0], "cat")) {
        if (argc != 2) {
            puts("Usage: cat <filename>\n");
        } else {
            fat16_print_file(argv[1]);
        }
    }
    else if (!strcmp(argv[0], "touch")) {
        if (argc != 2) {
            puts("Usage: touch <filename>\n");
        } else {
            fat16_create_file(argv[1]);
        }
    }
    else if (!strcmp(argv[0], "write")) {
        if (argc < 3) {
            puts("Usage: write <word> <filename>\n");
        } else {
            fat16_write_file(argv[2], argv[1], 0);
            puts("Written.\n");
        }
    }
    else if (!strcmp(argv[0], "echo")) {
        for (int i = 1; i < argc; i++) {
            puts(argv[i]);
            if (i < argc - 1) puts(" ");
        }
        puts("\n");
    }
    else if (!strcmp(argv[0], "clear")) {
        screen_clear();
    }
    else if (!strcmp(argv[0], "color")) {
        if (argc < 2) {
            puts("Usage: color <0-15>\n");
            puts("  0=black 1=blue 2=green 3=cyan 4=red 5=magenta\n");
            puts("  6=brown 7=gray 8=dk-gray 9=lt-blue 10=lt-green\n");
            puts("  11=lt-cyan 12=lt-red 13=pink 14=yellow 15=white\n");
        } else {
            int col = atoi(argv[1]);
            if (col < 0 || col > 15) {
                puts("Error: Color code must be between 0 and 15.\n");
            } else {
                set_print_color((uint16_t)col);
                puts("Color changed.\n");
            }
        }
    }
    else if (!strcmp(argv[0], "sysinfo")) {
        set_print_color(0x0B);
        puts("=== BenmoshOS System Info ===\n");
        set_print_color(0x0F);
        puts("  Architecture : x86 (32-bit protected mode)\n");
        puts("  Privilege    : Ring0 kernel + Ring3 user tasks\n");
        puts("  Memory model : Identity-mapped paging (0-8MB)\n");
        puts("    0x000000 - 0x3FFFFF : Kernel + stack space\n");
        puts("    0x400000 - 0x7FFFFF : Heap (4MB)\n");
        puts("    0x080000            : Ring3 user stack top\n");
        puts("  GDT segments :\n");
        puts("    0x08 kernel code  0x10 kernel data\n");
        puts("    0x1B user code    0x23 user data\n");
        puts("    0x28 TSS\n");
        puts("  Filesystem   : FAT16\n");
        puts("  Syscall gate : int 0x80\n");
    }
    else if (!strcmp(argv[0], "help")) {
        set_print_color(0x0E);
        puts("=== BenmoshOS Help ===\n");
        set_print_color(0x0F);
        puts("  help                - Show this help\n");
        puts("  ls                  - List files on disk\n");
        puts("  cat <file>          - Print file contents\n");
        puts("  touch <file>        - Create empty file\n");
        puts("  write <text> <file> - Write text to file\n");
        puts("  echo [text...]      - Print text\n");
        puts("  ps                  - List running tasks and CPU ticks\n");
        puts("  kill <pid>          - Terminate a running task\n");
        puts("  clear               - Clear the screen\n");
        puts("  color <0-15>        - Change text color\n");
        puts("  sysinfo             - Show system information\n");
        puts("  activate [program]  - Launch a program\n");
        puts("  vaddr_decode <addr> - Decode virtual address into PDI/PTI/Offset\n");
        puts("  heapdump            - Dump kernel heap blocks\n");
        puts("  meminfo             - Show physical frame statistics\n");
        puts("  exit                - Halt the operating system\n");
    }
    else if (!strcmp(argv[0], "activate")) {
        if (argc == 1) {
            set_print_color(0x0C);
            puts("ACTIVATING SELF DESTRUCT... just kidding :P\n");
            set_print_color(0x0F);
        } else if (argc > 2) {
            puts("Usage: activate <program>\n");
        } else {
            puts("Activating: ");
            puts(argv[1]);
            puts("\n");
            puts("(No programs loaded yet - use 'touch' to create files)\n");
        }
    }
    else if (!strcmp(argv[0], "exit")) {
        shell_exit();
    }
    else if (!strcmp(argv[0], "vaddr_decode")) {
        if (argc < 2) {
            puts("Usage: vaddr_decode <virtual_address>\n");
        } else {
            uint32_t virtual_address = (uint32_t)atoi(argv[1]);
            uint32_t pdi    = virtual_address >> 22;
            uint32_t pti    = (virtual_address >> 12) & 0x3FF;
            uint32_t offset = virtual_address & 0xFFF;

            printf("Virtual address: 0x%x (%d)\n", virtual_address, virtual_address);
            printf("  Page Directory Index (PDI): 0x%x (%d)\n", pdi, pdi);
            printf("  Page Table Index     (PTI): 0x%x (%d)\n", pti, pti);
            printf("  Page Offset               : 0x%x (%d)\n", offset, offset);
        }
    }
    else if (!strcmp(argv[0], "heapdump")) {
        extern struct block_header* heap_start;
        struct block_header* block = heap_start;
        int count = 0;

        while ((uint32_t)block < 0x800000) {
            uint32_t hdr = block->size_and_free;
            uint32_t size = hdr & ~1U;
            uint32_t free = hdr & 1U;

            if (size == 0) {
                break;
            }

            printf("[%d] 0x%x size=%d %s\n",
                   count++, (uint32_t)block, size, free ? "FREE" : "USED");

            block = (struct block_header*)((uint32_t)block + size + 8);
        }
    }
    else if (!strcmp(argv[0], "meminfo")) {
        uint32_t total_frames = 0;
        uint32_t free_frames  = 0;
        uint32_t used_frames  = 0;

        pmm_get_stats(&total_frames, &free_frames, &used_frames);
        printf("Physical Memory Info:\n");
        printf("  Total Frames: %d (0x%x)\n", total_frames, total_frames);
        printf("  Free Frames : %d (0x%x)\n", free_frames, free_frames);
        printf("  Used Frames : %d (0x%x)\n", used_frames, used_frames);
    }
    else if (!strcmp(argv[0], "ps")) {
        puts("PID    STATE    TICKS    STACK_TOP\n");
        puts("----------------------------------\n");
        uint32_t count = task_get_count();

        for (uint32_t i = 0; i < count; i++) {
            task_t* t = task_get(i);
            if (!t) continue;

            const char* state_str = "UNKNOWN";
            if (t->state == TASK_RUNNING)      state_str = "RUNNING";
            else if (t->state == TASK_READY)   state_str = "READY  ";
            else if (t->state == TASK_BLOCKED) state_str = "BLOCKED";
            else if (t->state == TASK_DEAD)    state_str = "DEAD   ";

            printf("%d      %s  %d        0x%x\n", t->pid, state_str, t->ticks, t->stack_top);
        }
    }
    else if (!strcmp(argv[0], "kill")) {
        if (argc != 2) {
            puts("Usage: kill <pid>\n");
        } else {
            uint32_t pid = (uint32_t)atoi(argv[1]);
            task_kill(pid);
            printf("Killed process %d\n", pid);
        }
    }
    else {
        puts("Unknown command: '");
        puts(argv[0]);
        puts("' (use 'help' for available commands)\n");
    }
}