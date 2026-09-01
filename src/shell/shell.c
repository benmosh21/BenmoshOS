#include "shell.h"


/* Forward declaration */
//static void print_shell_prompt();

void shell_exit() {
    // Replace print_string with your actual OS print/puts function name
    puts("\nShutting down BenmoshOS... Goodbye!\n");
    
    // Disable hardware interrupts and halt the CPU core
    __asm__ __volatile__("cli\n\t"
                         "hlt");
}

void execute_command(char* command) {
    char** argv = (char**)malloc(32 * sizeof(char*));
    if (argv == NULL) {
        puts("shell: out of memory\n");
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
            puts("Usage: ls      (no arguments)\n");
        } else {
            puts("Files on disk:\n");
            fat16_list_files();
        }
    }
    else if (!strcmp(argv[0], "cat")) {
        if (argc < 2) {
            puts("Usage: cat <filename>\n");
        } else if (argc > 2) {
            puts("Usage: cat <filename>   (one file at a time)\n");
        } else {
            fat16_print_file(argv[1]);
        }
    }
    else if (!strcmp(argv[0], "touch")) {
        if (argc < 2) {
            puts("Usage: wipe <filename>\n");
        } else if (argc > 2) {
            puts("Usage: wipe <filename>   (one file at a time)\n");
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
            
            int col = 0;
            for (int i = 0; argv[1][i] != '\0'; i++) {
                if (argv[1][i] >= '0' && argv[1][i] <= '9') {
                    col = col * 10 + (argv[1][i] - '0');
                }
            }
            if (col < 0 || col > 15) col = 15;
            set_print_color((uint16_t)col);
            puts("Color changed.\n");

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
        puts("  cat <file>          - print file contents\n");
        puts("  touch <file>        - Create empty file\n");
        puts("  write <text> <file> - Write text to file\n");
        puts("  echo [text...]      - print text\n");
        puts("  clear               - Clear the screen\n");
        puts("  color <0-15>        - Change text color\n");
        puts("  sysinfo             - Show system information\n");
        puts("  activate [program]  - Launch a program\n");
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
            puts("(No programs loaded yet — use 'wipe' to create files)\n");
        }
    }
    else if (!strcmp(argv[0], "exit")) {
        shell_exit();
        }
    else if (!strcmp(argv[0], "vaddr_decode")) {
            print_int(atoi(argv[1]));
            uint32_t virtual_address = atoi(argv[1]);
            printf("virtual address: %d\n", virtual_address);
            uint32_t pdi    = virtual_address >> 22;
            uint32_t pti    = (virtual_address >> 12) & 0x3FF;
            uint32_t offset = virtual_address & 0xFFF;

            printf("pdi: %x\n", pdi);
            printf("pti: %x\n", pti);
            printf("offset: %x\n", offset);
    }
    else if (!strcmp(argv[0], "heapdump")) {
        extern uint32_t* heap_start;
        uint32_t* block = heap_start;
        while(block < heap_start + )
    }
    else {
        puts("Unknown command: '");
        puts(argv[0]);
        puts("'  (use command 'help' for help)\n");
    }

    free(argv);
}
