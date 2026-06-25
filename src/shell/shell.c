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
        free(argv);
        return;
    }


    if (!strcmp(argv[0], "ls")) {
        if (argc > 1) {
            print("Usage: ls      (no arguments)\n");
        } else {
            print("Files on disk:\n");
            fat16_list_files();
        }
    }
    else if (!strcmp(argv[0], "cat")) {
        if (argc < 2) {
            print("Usage: cat <filename>\n");
        } else if (argc > 2) {
            print("Usage: cat <filename>   (one file at a time)\n");
        } else {
            fat16_print_file(argv[1]);
        }
    }
    else if (!strcmp(argv[0], "touch")) {
        if (argc < 2) {
            print("Usage: wipe <filename>\n");
        } else if (argc > 2) {
            print("Usage: wipe <filename>   (one file at a time)\n");
        } else {
            fat16_create_file(argv[1]);
        }
    }
    else if (!strcmp(argv[0], "write")) {
        if (argc < 3) {
            print("Usage: write <word> <filename>\n");
        } else {
            fat16_write_file(argv[2], argv[1], 0);
            print("Written.\n");
        }
    }
    else if (!strcmp(argv[0], "echo")) {
        for (int i = 1; i < argc; i++) {
            print(argv[i]);
            if (i < argc - 1) print(" ");
        }
        print("\n");
    }
    else if (!strcmp(argv[0], "clear")) {
        screen_clear();
    }
    else if (!strcmp(argv[0], "color")) {
        
        if (argc < 2) {
            print("Usage: color <0-15>\n");
            print("  0=black 1=blue 2=green 3=cyan 4=red 5=magenta\n");
            print("  6=brown 7=gray 8=dk-gray 9=lt-blue 10=lt-green\n");
            print("  11=lt-cyan 12=lt-red 13=pink 14=yellow 15=white\n");
        } else {
            
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
    else if (!strcmp(argv[0], "sysinfo")) {

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
    else if (!strcmp(argv[0], "stupid")) {
        /* stupid: show help */
        set_print_color(0x0E);
        print("=== BenmoshOS Help ===\n");
        set_print_color(0x0F);
        print("  help                - Show this help\n");
        print("  ls                  - List files on disk\n");
        print("  cat <file>          - Print file contents\n");
        print("  touch <file>        - Create empty file\n");
        print("  write <text> <file> - Write text to file\n");
        print("  echo [text...]      - Print text\n");
        print("  clear               - Clear the screen\n");
        print("  color <0-15>        - Change text color\n");
        print("  sysinfo             - Show system information\n");
        print("  activate [program]  - Launch a program\n");
    }
    else if (!strcmp(argv[0], "activate")) {
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
        print("'  (use command 'help' for help)\n");
    }

    free(argv);
}
