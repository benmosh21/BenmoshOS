#include "fat16.h"
#include "../drivers/ata/ata.h"      // Needed for ata_read_sector
#include "../drivers/print/print.h"  // Needed for your print function

// ==========================================
// --- HELPER FUNCTIONS ---
// ==========================================

// Compares two standard C strings. Returns 1 if they match perfectly, 0 if not.
int string_match(char* s1, char* s2) {
    int i = 0;
    while (s1[i] == s2[i]) {
        if (s1[i] == '\0') {
            return 1; // Match found
        }
        i++;
    }
    return 0; // Not a match
}

// Calculates the Microsoft-mandated 1-byte checksum of the 11-byte 8.3 short name.
// This checksum is required in every LFN ghost entry to bind it to the real file.
uint8_t lfn_checksum(const uint8_t* short_name) {
    uint8_t sum = 0;
    for (int i = 11; i != 0; i--) {
        // Rotate the bits right by 1, then add the next character's ASCII value
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *short_name++;
    }
    return sum;
}

// Takes a long string like "my_script.py" and turns it into "MYSCR~1  PY "
// Handles collisions by increasing the 'attempt' number (e.g., ~2, ~15)
void generate_short_name(char* long_name, uint8_t* short_name_out, int attempt) {
    // Fill the entire 11-byte output with spaces first
    for (int i = 0; i < 11; i++) short_name_out[i] = ' ';

    char base_name[8];
    int base_len = 0;
    int i = 0;

    // Extract the base name (skip spaces and dots, convert to UPPERCASE)
    while (long_name[i] != '\0' && long_name[i] != '.' && base_len < 8) {
        char c = long_name[i];
        if (c != ' ') {
            if (c >= 'a' && c <= 'z') c -= 32;
            base_name[base_len++] = c;
        }
        i++;
    }

    // Determine how many characters the "~X" suffix will take
    int suffix_len;
    if (attempt < 10) suffix_len = 2;       // e.g., "~1" (2 chars)
    else if (attempt < 100) suffix_len = 3; // e.g., "~10" (3 chars)
    else suffix_len = 4;                    // e.g., "~100" (4 chars)

    // Calculate how much of the base name we are allowed to keep without overflowing
    int keep_len = base_len;
    if (keep_len + suffix_len > 8) {
        keep_len = 8 - suffix_len;
    }

    // Write the valid part of the base name into the output
    for (int j = 0; j < keep_len; j++) short_name_out[j] = base_name[j];

    // Append the Tilde (~) and the actual attempt digits
    short_name_out[keep_len] = '~';
    if (attempt < 10) {
        short_name_out[keep_len + 1] = attempt + '0'; // Turn int into char
    }
    else if (attempt < 100) {
        short_name_out[keep_len + 1] = (attempt / 10) + '0';       // Tens
        short_name_out[keep_len + 2] = (attempt % 10) + '0';       // Ones
    }
    else {
        short_name_out[keep_len + 1] = (attempt / 100) + '0';      // Hundreds
        short_name_out[keep_len + 2] = ((attempt / 10) % 10) + '0';// Tens
        short_name_out[keep_len + 3] = (attempt % 10) + '0';       // Ones
    }

    // Find the last dot in the original string to find the extension
    int last_dot = -1;
    int len = 0;
    while (long_name[len] != '\0') {
        if (long_name[len] == '.') last_dot = len;
        len++;
    }

    // Copy up to 3 characters of the extension into the last 3 slots
    if (last_dot != -1) {
        int ext_idx = 8;
        i = last_dot + 1;
        while (long_name[i] != '\0' && ext_idx < 11) {
            char c = long_name[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            short_name_out[ext_idx++] = c;
            i++;
        }
    }
}


// ==========================================
// --- MAIN FAT16 DRIVER COMMANDS ---
// ==========================================

// Finds a file (LFN or short) and prints its text.
void fat16_print_file(char* target_file) {
    uint8_t buffer[512];
    char lfn_buffer[256];

    // Ensure our long file name buffer starts completely empty
    for (int i = 0; i < 256; i++) lfn_buffer[i] = '\0';

    // Read Sector 0 (BPB) to get the hard drive blueprints
    ata_read_sector(0, buffer);
    bpb_t* bpb = (bpb_t*)buffer;

    // Calculate where the Root Directory and Data Clusters live on the disk
    uint32_t root_dir_sector = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    uint32_t root_dir_size = (bpb->root_dir_entries * 32) / 512;
    uint32_t data_sector = root_dir_sector + root_dir_size;
    uint8_t clusters_size = bpb->sectors_per_cluster;
    uint32_t fat_start_sector = bpb->reserved_sectors;

    int file_found = 0;
    uint16_t target_cluster = 0;
    uint32_t directory_entry_file_size = 0;

    // Search the Root Directory sectors
    for (uint32_t s = 0; s < root_dir_size; s++) {
        ata_read_sector(root_dir_sector + s, buffer);
        dir_entry_t* directory = (dir_entry_t*)buffer;

        // Loop through the 16 entries (32 bytes each) in this 512-byte sector
        for (int i = 0; i < 16; i++) {
            uint8_t first_byte = directory[i].filename[0];

            if (first_byte == 0x00) break; // End of directory
            if (first_byte == 0xE5) {
                // Deleted file. Clear buffer in case we read orphaned ghost entries.
                lfn_buffer[0] = '\0';
                continue;
            }

            // Check if this is an LFN Ghost Entry (Attribute 0x0F)
            if (directory[i].attributes == 0x0F) {
                struct lfn_entry* lfn = (struct lfn_entry*)&directory[i];

                // Mask with 0x1F to remove the 0x40 "Last Entry" flag and get the true index
                int index = (lfn->sequence_number & 0x1F) - 1;

                // Extract the UTF-16 characters and convert them to standard 1-byte ASCII
                if (index >= 0 && index < 20) {
                    int offset = index * 13;
                    for (int j = 0; j < 5; j++) lfn_buffer[offset++] = (lfn->name1[j] == 0xFFFF) ? '\0' : (char)(lfn->name1[j] & 0xFF);
                    for (int j = 0; j < 6; j++) lfn_buffer[offset++] = (lfn->name2[j] == 0xFFFF) ? '\0' : (char)(lfn->name2[j] & 0xFF);
                    for (int j = 0; j < 2; j++) lfn_buffer[offset++] = (lfn->name3[j] == 0xFFFF) ? '\0' : (char)(lfn->name3[j] & 0xFF);
                }
            }
            // If it is NOT a volume label (0x08), it is the real file entry!
            else if (directory[i].attributes != 0x08) {

                // If we successfully collected a long name, check if it matches target_file
                if (lfn_buffer[0] != '\0') {
                    if (string_match(lfn_buffer, target_file) == 1) {
                        target_cluster = directory[i].starting_cluster;
                        directory_entry_file_size = directory[i].file_size;
                        file_found = 1;
                        break;
                    }
                }

                // Clear the LFN buffer for the next file
                for (int reset = 0; reset < 256; reset++) lfn_buffer[reset] = '\0';
            }
        }
        if (file_found == 1) break; // Stop reading sectors if we found it
    }

    // If the file was found, read its text data by following the FAT cluster chain
    if (file_found == 1) {
        uint16_t current_cluster = target_cluster;
        uint32_t bytes_remaining = directory_entry_file_size;

        // 0xFFF8 marks the end of a file in the FAT table
        while (current_cluster < 0xFFF8 && bytes_remaining > 0) {
            uint32_t cluster_offset = current_cluster - 2; // Clusters start at #2
            uint32_t file_lba = data_sector + (cluster_offset * clusters_size);

            ata_read_sector(file_lba, buffer); // Pull the text data into memory

            // Print the characters to the screen
            for (int j = 0; j < 512 && bytes_remaining > 0; j++) {
                char current_char = buffer[j];

                // Ignore carriage returns to prevent weird spacing
                if (current_char == '\r') {
                    bytes_remaining--;
                    continue;
                }

                char temp_string[2];
                temp_string[0] = buffer[j];
                temp_string[1] = '\0';
                print(temp_string);

                bytes_remaining--;
            }

            // Look up the NEXT cluster address in the File Allocation Table (FAT)
            uint32_t fat_sector_offset = (current_cluster * 2) / 512;
            uint32_t fat_byte_offset = (current_cluster * 2) % 512;

            ata_read_sector(fat_start_sector + fat_sector_offset, buffer);
            uint16_t* fat_table = (uint16_t*)buffer;
            current_cluster = fat_table[fat_byte_offset / 2];
        }
        print("\n");
    }
    else {
        print("File not found!\n");
    }
}

// Scans the root directory and prints all filenames.
void fat16_list_files() {
    uint8_t buffer[512];
    char lfn_buffer[256];

    for (int i = 0; i < 256; i++) lfn_buffer[i] = '\0';

    ata_read_sector(0, buffer);
    bpb_t* bpb = (bpb_t*)buffer;

    uint32_t root_dir_sector = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    uint32_t sectors_to_read = (bpb->root_dir_entries * 32) / 512;

    for (uint32_t s = 0; s < sectors_to_read; s++) {
        ata_read_sector(root_dir_sector + s, buffer);
        dir_entry_t* directory = (dir_entry_t*)buffer;

        for (int i = 0; i < 16; i++) {
            uint8_t first_byte = directory[i].filename[0];

            if (first_byte == 0x00) return; // Directory is completely empty from here on

            if (first_byte == 0xE5) { // File is deleted
                lfn_buffer[0] = '\0';
                continue;
            }

            // Extract LFN characters from Ghost Entries (0x0F)
            if (directory[i].attributes == 0x0F) {
                struct lfn_entry* lfn = (struct lfn_entry*)&directory[i];
                int index = (lfn->sequence_number & 0x1F) - 1;

                if (index >= 0 && index < 20) {
                    int offset = index * 13;
                    for (int j = 0; j < 5; j++) lfn_buffer[offset++] = (lfn->name1[j] == 0xFFFF) ? '\0' : (char)(lfn->name1[j] & 0xFF);
                    for (int j = 0; j < 6; j++) lfn_buffer[offset++] = (lfn->name2[j] == 0xFFFF) ? '\0' : (char)(lfn->name2[j] & 0xFF);
                    for (int j = 0; j < 2; j++) lfn_buffer[offset++] = (lfn->name3[j] == 0xFFFF) ? '\0' : (char)(lfn->name3[j] & 0xFF);
                }
            }
            // We reached the actual standard file entry
            else if (directory[i].attributes != 0x08) {
                if (lfn_buffer[0] != '\0') {
                    // Print the beautifully constructed Long File Name
                    print(lfn_buffer);
                }
                else {
                    // Fallback: No ghost entries found, print the strict 8.3 name
                    for (int j = 0; j < 8; j++) {
                        if (directory[i].filename[j] != ' ') {
                            char temp[2] = { directory[i].filename[j], '\0' };
                            print(temp);
                        }
                    }
                    if (directory[i].extension[0] != ' ') {
                        print(".");
                        for (int j = 0; j < 3; j++) {
                            if (directory[i].extension[j] != ' ') {
                                char temp[2] = { directory[i].extension[j], '\0' };
                                print(temp);
                            }
                        }
                    }
                }

                // Drop a line for the next file and reset the buffer
                print("\n");
                for (int reset = 0; reset < 256; reset++) {
                    lfn_buffer[reset] = '\0';
                }
            }
        }
    }
}

// Creates a completely new, empty file with full LFN support.
void fat16_create_file(char* target_name) {
    uint8_t buffer[512];
    ata_read_sector(0, buffer);
    bpb_t* bpb = (bpb_t*)buffer;

    uint32_t root_dir_sector = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    uint32_t sectors_to_read = (bpb->root_dir_entries * 32) / 512;

    // Calculate how many characters and how many memory slots do we need?
    int name_len = 0;
    while (target_name[name_len] != '\0') name_len++;

    int lfn_count = (name_len + 12) / 13; // Integer math trick to round up division
    int total_slots = lfn_count + 1;      // Total = Ghost entries + 1 standard 8.3 entry

    // COLLISION SCANNER. Find a unique 8.3 short name (e.g., MYSCR~1, MYSCR~2)
    uint8_t short_name[11];
    int attempt = 1;
    int collision = 1;

    while (collision) {
        generate_short_name(target_name, short_name, attempt);
        collision = 0;

        // Scan the entire directory to see if this short name is already taken
        for (uint32_t s = 0; s < sectors_to_read; s++) {
            ata_read_sector(root_dir_sector + s, buffer);
            dir_entry_t* dir = (dir_entry_t*)buffer;

            for (int i = 0; i < 16; i++) {
                if (dir[i].filename[0] == 0x00) break;
                if (dir[i].attributes != 0x0F && dir[i].attributes != 0x08 && dir[i].filename[0] != 0xE5) {
                    int match = 1;
                    // Check if the current file matches our generated short name
                    for (int k = 0; k < 8; k++) if (dir[i].filename[k] != short_name[k]) match = 0;
                    for (int k = 0; k < 3; k++) if (dir[i].extension[k] != short_name[8 + k]) match = 0;
                    if (match) { collision = 1; break; }
                }
            }
            if (collision) break; // Break out of sector loop if collision found
        }
        if (collision) attempt++; // Try the next number!
    }

    // Now that we have a safe short name, generate the LFN checksum for the ghosts
    uint8_t chksum = lfn_checksum(short_name);

    // FIND FREE MEMORY. Look for perfect consecutive 0x00 or 0xE5 slots
    int found_index = -1;
    int consecutive = 0;

    for (uint32_t s = 0; s < sectors_to_read; s++) {
        ata_read_sector(root_dir_sector + s, buffer);
        for (int i = 0; i < 16; i++) {
            uint8_t first_byte = buffer[i * 32]; // Look at the first byte of each 32-byte chunk

            if (first_byte == 0x00 || first_byte == 0xE5) {
                consecutive++;
                if (consecutive == total_slots) {
                    // We found enough space! Calculate the absolute index of where the first ghost goes
                    int current_absolute_index = (s * 16) + i;
                    found_index = current_absolute_index - total_slots + 1;
                    break;
                }
            }
            else {
                consecutive = 0; // Combo broken by an existing file! Reset the count.
            }
        }
        if (found_index != -1) break;
    }

    if (found_index == -1) {
        print("Error: Root directory is completely full or fragmented!\n");
        return;
    }

    // WRITE TO DISK. Inject the ghost entries and the real file entry
    for (int i = 0; i < total_slots; i++) {
        int absolute_idx = found_index + i;
        uint32_t sec = root_dir_sector + (absolute_idx / 16);
        uint32_t off = (absolute_idx % 16) * 32;

        ata_read_sector(sec, buffer); // Pull the sector into memory

        if (i < lfn_count) {
            // Write a Ghost Entry (Stacking downwards)
            int lfn_idx = lfn_count - i; // Example: 3, then 2, then 1
            int char_offset = (lfn_idx - 1) * 13;

            struct lfn_entry* lfn = (struct lfn_entry*)(buffer + off);
            lfn->sequence_number = lfn_idx;
            if (i == 0) lfn->sequence_number |= 0x40; // The very top entry needs the 0x40 flag
            lfn->attribute = 0x0F;
            lfn->type = 0;
            lfn->checksum = chksum;
            lfn->first_cluster = 0;

            // Generate the UTF-16 characters and 0xFFFF padding
            uint16_t chars[13];
            for (int c = 0; c < 13; c++) {
                int pos = char_offset + c;
                if (pos < name_len) chars[c] = target_name[pos];
                else if (pos == name_len) chars[c] = 0x0000; // Null terminator
                else chars[c] = 0xFFFF; // FAT16 padding requirement
            }

            // Map the 13 characters into the weird LFN struct spacing
            for (int c = 0; c < 5; c++) lfn->name1[c] = chars[c];
            for (int c = 0; c < 6; c++) lfn->name2[c] = chars[5 + c];
            for (int c = 0; c < 2; c++) lfn->name3[c] = chars[11 + c];

        }
        else {
            // Write the Standard 8.3 Entry at the very bottom of the stack
            dir_entry_t* dir = (dir_entry_t*)(buffer + off);
            for (int j = 0; j < 8; j++) dir->filename[j] = short_name[j];
            for (int j = 0; j < 3; j++) dir->extension[j] = short_name[8 + j];
            dir->attributes = 0x20; // Normal archive file
            for (int j = 0; j < 10; j++) dir->reserved[j] = 0;
            dir->time = 0;
            dir->date = 0;
            dir->starting_cluster = 0; // 0 because there is no text in it yet
            dir->file_size = 0;
        }

        ata_write_sector(sec, buffer); // Save the modifications to the hard drive!
    }

    print("Long File created successfully!\n");
}


void fat16_write_file(char* target_file, char* data, int rewriting) {
    uint8_t buffer[512];
    char lfn_buffer[256];
    for (int i = 0; i < 256; i++) lfn_buffer[i] = '\0';

    ata_read_sector(0, buffer);
    bpb_t* bpb = (bpb_t*)buffer;

    uint32_t root_dir_sector = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    uint32_t root_dir_size = (bpb->root_dir_entries * 32) / 512;
    uint32_t data_sector = root_dir_sector + root_dir_size;
    uint32_t fat_start_sector = bpb->reserved_sectors;

    // Save the clusters size befure the buffer gets overwitten.
    uint8_t clusters_size = bpb->sectors_per_cluster;

    int file_found = 0;
    uint32_t target_dir_sector = 0;
    int target_dir_index = 0;

    // 1. FIND THE FILE IN THE ROOT DIRECTORY
    for (uint32_t s = 0; s < root_dir_size; s++) {
        ata_read_sector(root_dir_sector + s, buffer);
        dir_entry_t* directory = (dir_entry_t*)buffer;

        for (int i = 0; i < 16; i++) {
            if (directory[i].filename[0] == 0x00) break;
            if (directory[i].filename[0] == 0xE5) {
                lfn_buffer[0] = '\0';
                continue;
            }

            if (directory[i].attributes == 0x0F) {
                struct lfn_entry* lfn = (struct lfn_entry*)&directory[i];
                int index = (lfn->sequence_number & 0x1F) - 1;
                if (index >= 0 && index < 20) {
                    int offset = index * 13;
                    for (int j = 0; j < 5; j++) lfn_buffer[offset++] = (lfn->name1[j] == 0xFFFF) ? '\0' : (char)(lfn->name1[j] & 0xFF);
                    for (int j = 0; j < 6; j++) lfn_buffer[offset++] = (lfn->name2[j] == 0xFFFF) ? '\0' : (char)(lfn->name2[j] & 0xFF);
                    for (int j = 0; j < 2; j++) lfn_buffer[offset++] = (lfn->name3[j] == 0xFFFF) ? '\0' : (char)(lfn->name3[j] & 0xFF);
                }
            }
            else if (directory[i].attributes != 0x08) {
                if (lfn_buffer[0] != '\0' && string_match(lfn_buffer, target_file) == 1) {
                    target_dir_sector = root_dir_sector + s;
                    target_dir_index = i;
                    file_found = 1;
                    break;
                }
                for (int reset = 0; reset < 256; reset++) lfn_buffer[reset] = '\0';
            }
        }
        if (file_found) break;
    }

    if (!file_found) {
        print("Error: File not found. Use 'wipe' to create it first.\n");
        return;
    }

    // 2. CHECK IF WE NEED TO ALLOCATE A CLUSTER
        // Get the file's current stats directly from the directory entry
    dir_entry_t* target_entry = (dir_entry_t*)(buffer + (target_dir_index * 32));
    uint16_t file_cluster = target_entry->starting_cluster;
    uint32_t file_size = target_entry->file_size;

    // If the file is completely empty (size 0, no cluster), find it a fresh home
    if (file_cluster == 0) {
        ata_read_sector(fat_start_sector, buffer);
        uint16_t* fat_table = (uint16_t*)buffer;
        for (int i = 2; i < 256; i++) {
            if (fat_table[i] == 0x0000) {
                file_cluster = i;
                fat_table[i] = 0xFFFF; // Mark EOF
                ata_write_sector(fat_start_sector, buffer); // Save FAT
                break;
            }
        }
        if (file_cluster == 0) { print("Error: Disk is full!\n"); return; }
        file_size = 0; // It's a fresh cluster
    }

    // 3. READ, MODIFY, AND WRITE THE DATA
    uint32_t cluster_offset = file_cluster - 2;
    uint32_t file_lba = data_sector + (cluster_offset * clusters_size);

    // Pull the file's current text off the metal
    ata_read_sector(file_lba, buffer);

    int write_offset = 0;
    int data_len = strlen(data);

    if (!rewriting && file_size > 0) {
        // APPEND MODE: Start writing at the end of the existing file size
        write_offset = file_size;

        // Add a newline before appending so it doesn't mash into the previous sentence
        buffer[write_offset] = '\n';
        write_offset++;

        // Safety check to prevent overflowing a 512-byte sector
        if (write_offset + data_len >= 512) {
            print("Error: File exceeds 1 sector. Multi-sector appending not supported yet!\n");
            return;
        }
    }
    else {
        // OVERWRITE MODE: Erase the buffer back to zeros
        for (int i = 0; i < 512; i++) buffer[i] = 0;
        write_offset = 0;
    }

    // Copy the new text into the buffer and write to disk
    memcpy(buffer + write_offset, (uint8_t*)data, data_len);
    ata_write_sector(file_lba, buffer);

    // 4. UPDATE ROOT DIRECTORY
    ata_read_sector(target_dir_sector, buffer);
    dir_entry_t* dir = (dir_entry_t*)buffer;

    dir[target_dir_index].starting_cluster = file_cluster;
    dir[target_dir_index].file_size = write_offset + data_len;

    ata_write_sector(target_dir_sector, buffer);
    print("Data written successfully!\n");
}