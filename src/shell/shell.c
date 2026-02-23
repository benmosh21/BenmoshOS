#include "shell.h"

void execute_command(char* command) {
	char* argv[10]; // Array to hold command arguments
	int argc = 0; // Argument count
	int in_word = 0;
	for (int i = 0; command[i] != '\0'; i++) {
		if (command[i] == '\n') {
			in_word = 0; // End of a word
			command[i] = '\0'; // Null-terminate the argument
		} else if (command[i] == ' ') {
			in_word = 0; // End of a word
			command[i] = '\0'; // Null-terminate the argument
		}
		else if (in_word == 0) {
			in_word = 1; // Start of a new word
			argv[argc++] = &command[i]; // Store the argument
		}
	}

	if (argc == 0) {
		print("what a loser, you dont even know how to type a command :P\n");
		return; // No command entered
	}
	else if (strcmp(argv[0], "purge")) {
		if (argc > 1) {
			print("Usage: purge (no arguments allowed)\n");
		}
		else {
			print("Files on disk: \n");
			fat16_list_files();
		}
	}
	else if (strcmp(argv[0], "void")) {
		if (argc < 2) {
			print("Usage: void <filename>\n");
		} else if (argc > 2) {
			print("Usage: void <filename> (only one filename allowed)\n");
		}
		else {
			fat16_print_file(argv[1]);
		}
	}
	else if (strcmp(argv[0], "wipe")) {
		if (argc < 2) {
			print("Usage: wipe <filename>\n");
		}
		else if (argc > 2) {
			print("Usage: wipe <filename> (only one filename allowed)\n");
		}
		else {
			fat16_create_file(argv[1]);
		}
	} 
	else if (strcmp(argv[0], "write")) {
		if (argc < 2) {
			print("ok, lets write ...\n");
			print("wait i dont see what to write, ok go to help\n");
		}
		else if (argc == 2) {
			print(argv[1]);
			print("\n");
		}
		else if (argc == 3) {
			fat16_write_file(argv[2], argv[1], 0);
		}
	}
	else if (strcmp(argv[0], "stupid")) {
		if (argc > 1) {
			print("Usage: you stupid, (no arguments allowed)\n");
			return;
		}
		print("How stupid are you:\n");
		print("purge - List files in the current directory\n");
		print("void <filename> - Display the contents of a file\n");
		print("wipe <filename> - Create a new empty file\n");
		print("stupid - Show this help message\n");
	}
	else {
		print("what a loser, you dont even know how to type a real command :P\n");
		print("Try using the 'help' command to see a list of available commands.\n");
	} 

}