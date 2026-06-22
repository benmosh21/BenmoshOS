/*
 * shell.c - BenmoshOS command shell
 *
 * FIXES:
 *  - strcmp returns 1 on MATCH, 0 on mismatch.
 *    All if(strcmp(...)) checks were logically inverted — the "else" branch
 *    was executing on a match, and the "if" branch on a mismatch.
 *    Fixed: changed every if(strcmp(argv[0],"cmd")) to if(!strcmp(argv[0],"cmd"))
 *    so the logic reads: "if NOT a match, skip to next elif".
 *    Actually cleaner fix: check == 1 for match.
 *
 * NEW COMMANDS:
 *  - clear    : clear the screen
 *  - color N  : change text color (0-15)
 *  - echo     : print arguments
 *  - sysinfo  : show system info
 */

#include "shell.h"
#include "../memory/heap/heap.h"
#include "../kernel/system.h"

/* Forward declaration */
static void print_shell_prompt();

void execute_command(char* command) {
    char** argv = (char**)malloc(32 * sizeof(char*));
    if (argv == NULL) {
        print("shell: out of memory\n");
        return;
    }

    int argc    = 0;
    int in_word = 0;

    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] == '\n' || command[i] == ' ') {
            in_word    = 0;
            command[i] = '\0';
        } else if (in_word == 0) {
            in_word       = 1;
            argv[argc++]  = &command[i];
        }
    }

    if (argc == 0) {
        /* Empty enter — just reprint the prompt, no insult needed */
        free(argv);
        return;
    }

    /* ---- Command dispatch ----
     * strcmp() returns 1 on match, 0 on no-match.
     * We use (strcmp(argv[0], "cmd") == 1) for clarity. */

    if (strcmp(argv[0], "purge") == 1) {
        /* purge: list files on disk */
        if (argc > 1) {
            print("Usage: purge   (no arguments)\n");
        } else {
            print("Files on disk:\n");
            fat16_list_files();
        }
    }
    else if (strcmp(argv[0], "void") == 1) {
        /* void <filename>: print file contents */
        if (argc < 2) {
            print("Usage: void <filename>\n");
        } else if (argc > 2) {
            print("Usage: void <filename>   (one file at a time)\n");
        } else {
            fat16_print_file(argv[1]);
        }
    }
    else if (strcmp(argv[0], "wipe") == 1) {
        /* wipe <filename>: create empty file */
        if (argc < 2) {
            print("Usage: wipe <filename>\n");
        } else if (argc > 2) {
            print("Usage: wipe <filename>   (one file at a time)\n");
        } else {
            fat16_create_file(argv[1]);
        }
    }
    else if (strcmp(argv[0], "write") == 1) {
        /* write <word> <filename>: write word into file */
        if (argc < 3) {
            print("Usage: write <word> <filename>\n");
        } else {
            fat16_write_file(argv[2], argv[1], 0);
            print("Written.\n");
        }
    }
    else if (strcmp(argv[0], "echo") == 1) {
        /* echo [args...]: print arguments */
        for (int i = 1; i < argc; i++) {
            print(argv[i]);
            if (i < argc - 1) print(" ");
        }
        print("\n");
    }
    else if (strcmp(argv[0], "clear") == 1) {
        /* clear: clear the screen */
        screen_clear();
    }
    else if (strcmp(argv[0], "color") == 1) {
        /* color <0-15>: change text color */
        if (argc < 2) {
            print("Usage: color <0-15>\n");
            print("  0=black 1=blue 2=green 3=cyan 4=red 5=magenta\n");
            print("  6=brown 7=gray 8=dk-gray 9=lt-blue 10=lt-green\n");
            print("  11=lt-cyan 12=lt-red 13=pink 14=yellow 15=white\n");
        } else {
            /* Parse the number from argv[1] */
            int col = 0;
            for (int i = 0; argv[1][i] != '\0'; i++) {
                if (argv[1][i] >= '0' && argv[1][i] <= '9') {
                    col = col * 10 + (argv[1][i] - '0');
                }
            }
            if (col < 0 || col > 15) col = 15;
            set_print_color((uint16_t)col);
            print("Color changed.\n");
        }
    }
    else if (strcmp(argv[0], "sysinfo") == 1) {
        /* sysinfo: print system information */
        set_print_color(0x0B);
        print("=== BenmoshOS System Info ===\n");
        set_print_color(0x0F);
        print("  Architecture : x86 (32-bit protected mode)\n");
        print("  Privilege    : Ring0 kernel + Ring3 user tasks\n");
        print("  Memory model : Identity-mapped paging (0-8MB)\n");
        print("    0x000000 - 0x3FFFFF : Kernel + stack space\n");
        print("    0x400000 - 0x7FFFFF : Heap (4MB)\n");
        print("    0x080000            : Ring3 user stack top\n");
        print("  GDT segments :\n");
        print("    0x08 kernel code  0x10 kernel data\n");
        print("    0x1B user code    0x23 user data\n");
        print("    0x28 TSS\n");
        print("  Filesystem   : FAT16\n");
        print("  Syscall gate : int 0x80\n");
    }
    else if (strcmp(argv[0], "stupid") == 1) {
        /* stupid: show help */
        set_print_color(0x0E);
        print("=== BenmoshOS Help ===\n");
        set_print_color(0x0F);
        print("  purge               - List files on disk\n");
        print("  void <file>         - Print file contents\n");
        print("  wipe <file>         - Create empty file\n");
        print("  write <text> <file> - Write text to file\n");
        print("  echo [text...]      - Print text\n");
        print("  clear               - Clear the screen\n");
        print("  color <0-15>        - Change text color\n");
        print("  sysinfo             - Show system information\n");
        print("  activate [program]  - Launch a program\n");
        print("  stupid              - Show this help\n");
    }
    else if (strcmp(argv[0], "activate") == 1) {
        if (argc == 1) {
            set_print_color(0x0C);
            print("ACTIVATING SELF DESTRUCT... just kidding :P\n");
            set_print_color(0x0F);
        } else if (argc > 2) {
            print("Usage: activate <program>\n");
        } else {
            print("Activating: ");
            print(argv[1]);
            print("\n");
            print("(No programs loaded yet — use 'wipe' to create files)\n");
        }
    }
    else {
        print("Unknown command: '");
        print(argv[0]);
        print("'  (type 'stupid' for help)\n");
    }

    free(argv);
}
