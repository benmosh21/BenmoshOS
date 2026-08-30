#include "fat16.h"

#define ATA_SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32
#define DIRECTORY_SIZE 4096

static bpb_t fat_bpb;
static uint32_t fat_start_sector;
static uint32_t root_dir_start_sector;
static uint32_t data_start_sector;


static int fat_strcasecmp(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? (char)(*a - 32) : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? (char)(*b - 32) : *b;
        if (ca != cb) return 1;
        a++; b++;
    }
    return *a != *b;
}

uint32_t get_next_segment(const char* path, uint32_t* pathIndex, char* outBuffer) {
    uint32_t counter = 0;

    // Skip any '/' in the start of the folder
    while (path[pathIndex[0]] == '/') {
        pathIndex[0]++;
    }

    if (path[pathIndex[0]] == '\0') {
        return 2;
    }

    // Add each char into the buffer untile null byter for end of path or / for end of directory name
    while (path[pathIndex[0] + counter] != '/' && path[pathIndex[0] + counter] != '\0') {
        // Prevent bounds overflow
        if (counter < 255) {
            outBuffer[counter] = path[pathIndex[0] + counter];
        }
        counter++;
    }

    if (path[pathIndex[0] + counter] == '\0') {
        // Add null terminator to the buffer
        outBuffer[counter < 255 ? counter : 255] = '\0';

        // find the new pathIndex
        pathIndex[0] += counter;

        return 1;
    }

    // Add null terminator to the buffer
    outBuffer[counter < 255 ? counter : 255] = '\0';

    // find the new pathIndex
    pathIndex[0] += counter;

    return 0;
}

// Returns 0 on success, 1 if the name is invalid for an 8.3 format
uint32_t sfnEncoder(char* name, char* sfn) {
    /*
    * A standard 8.3 name has a strict maximum of 12 characters:
    * 8 for the base name + 1 dot + 3 for the extension.
    */
    if (strlen(name) > 12) {
        return 1;
    }

    /*
    * Pre-fill the 11-byte output array entirely with spaces.
    * This automatically handles files that don't have extensions!
    */
    memset((unsigned char*)sfn, ' ', 11);

    uint32_t r = 0; // Read index to track where we are in 'name'
    uint32_t w = 0; // Write index to track where we are in 'sfn'
    uint8_t dot = 0;

    while (name[r] != '\0') {
        if (name[r] == '.') {
            if (dot == 1) {
                return 1; // Error: Standard SFNs cannot contain multiple dots
            }
            dot = 1;
            w = 8; // Jump the write index directly to the 3-byte extension block
            r++;   // Move past the dot in the read string so we don't copy it
            continue;
        }

        // Boundary checks to prevent overflowing the 8-byte and 3-byte segments
        if (dot == 0 && w >= 8) {
            return 1; // Error: Base name exceeds 8 characters
        }
        if (dot == 1 && w >= 11) {
            return 1; // Error: Extension exceeds 3 characters
        }

        // Safely convert lowercase letters to uppercase
        if (name[r] >= 'a' && name[r] <= 'z') {
            sfn[w] = name[r] - 32;
        }
        else {
            sfn[w] = name[r]; // Copy numbers and uppercase letters exactly as they are
        }

        r++;
        w++;
    }

    return 0; // Success
}


// Stateful LFN parsing: Returns index on match, -1 on end of directory, -2 to keep searching.
int32_t findEntryInBuffer(char* name, char* formatName, dir_entry_t* entries, uint32_t numEntries, lfn_state_t* state) {

    for (uint32_t i = 0; i < numEntries; i++) {

        // Stop scanning if we hit end of directory
        if (entries[i].filename[0] == 0x00) {
            return -1;
        }

        if (entries[i].filename[0] == 0xE5) {
            state->checksum = 0;
            state->rightChecksum = 0;
            memset((unsigned char*)state->lfn_buffer, 0, sizeof(state->lfn_buffer));
            continue;
        }

        if (entries[i].attributes == 0x0F) {
            struct lfn_entry* lfn = (struct lfn_entry*)&entries[i];

            int32_t chunkIndex = (lfn->sequence_number & 0x1F) - 1;

            // Safety bounds check for the chunk index
            if (chunkIndex >= 0 && chunkIndex < 20) {
                uint32_t offset = chunkIndex * 13;

                // Set the lfn_buffer while skipping the 0x0000 terminator and 0xFFFF padding
                for (uint32_t j = 0; j < 5; j++) {
                    if (lfn->name1[j] != 0xFFFF && lfn->name1[j] != 0x0000) {
                        state->lfn_buffer[offset++] = (char)(lfn->name1[j] & 0xFF);
                    }
                }
                for (uint32_t j = 0; j < 6; j++) {
                    if (lfn->name2[j] != 0xFFFF && lfn->name2[j] != 0x0000) {
                        state->lfn_buffer[offset++] = (char)(lfn->name2[j] & 0xFF);
                    }
                }
                for (uint32_t j = 0; j < 2; j++) {
                    if (lfn->name3[j] != 0xFFFF && lfn->name3[j] != 0x0000) {
                        state->lfn_buffer[offset++] = (char)(lfn->name3[j] & 0xFF);
                    }
                }
            }
            // Verify that all ghost entries in this stack share the exact same checksum
            if (state->rightChecksum == 0) {
                state->checksum = lfn->checksum;
                state->rightChecksum = 1;
            }
            else if (state->rightChecksum == 1) {
                if (state->checksum != lfn->checksum) {
                    state->rightChecksum = 2; // Checksum corrupted, mark as invalid
                    memset((unsigned char*)state->lfn_buffer, 0, sizeof(state->lfn_buffer));
                }
            }
        } // End of if

        // standard 8.3 SFN entry
        else {
            uint32_t lfnMatch = 0;
            uint32_t sfnMatch = 0;

            // Check if the LFN matched
            if (state->rightChecksum == 1 && fat_strcasecmp(name, state->lfn_buffer) == 0) {
                uint8_t sfn_checksum = 0;
                for (uint32_t j = 0; j < 11; j++) {
                    sfn_checksum = ((sfn_checksum & 1) ? 0x80 : 0) + (sfn_checksum >> 1) + entries[i].short_name[j];
                }
                if (state->checksum == sfn_checksum) {
                    lfnMatch = 1;
                }
            }

            // Check if the SFN matched
            char targetSFN[11];
            if (sfnEncoder(name, targetSFN) == 0) {
                if (memcmp(targetSFN, entries[i].short_name, 11) == 0) {
                    sfnMatch = 1;
                }
            }

            // return success if either matched
            if (sfnMatch || lfnMatch) {
                memcpy((uint8_t*)formatName, (uint8_t*)entries[i].short_name, 11);
                return (int32_t)i;
            }
            // if not then we do the check again
            else {
                state->rightChecksum = 0;
                memset((unsigned char*)state->lfn_buffer, 0, sizeof(state->lfn_buffer));
            }
        } // End of else if
    } // End of for loop

    return -2; // Not found

} // End of getFormatName


uint32_t fat16_init() {
    char buffer[ATA_SECTOR_SIZE];

    // Check for read errors
    if (ata_read_sector(0, (uint8_t*)buffer) != 0) return 1;

    memcpy((unsigned char*)&fat_bpb, (const uint8_t*)buffer, sizeof(fat_bpb));

    // Ensure FAT format uses 512-byte sectors to match our ATA driver
    if (fat_bpb.bytes_per_sector != ATA_SECTOR_SIZE) return 1;

    if (fat_bpb.sectors_per_cluster == 0 || fat_bpb.fat_count == 0 ||
        fat_bpb.sectors_per_fat == 0 || fat_bpb.root_dir_entries) return 1;

    // calculate the start of the fat & root_directory
    fat_start_sector = fat_bpb.reserved_sectors;
    root_dir_start_sector = fat_start_sector + (fat_bpb.fat_count * fat_bpb.sectors_per_fat);

    // Prevent truncation by safely rounding up
    uint32_t root_dir_bytes = fat_bpb.root_dir_entries * DIR_ENTRY_SIZE;
    uint32_t root_dir_sectors = (root_dir_bytes + fat_bpb.bytes_per_sector - 1) / fat_bpb.bytes_per_sector;

    data_start_sector = root_dir_start_sector + root_dir_sectors;

    return 0;
}




uint32_t fat_read_fileT(char* path, char* fileBuffer, uint32_t maxBufferSize, uint32_t* outFileSize) {

    // Check if path and fileBuffer exist
    if (!path || !fileBuffer || !outFileSize) return 1;

    static char lastDirectoryBuffer[DIRECTORY_SIZE];

    // Declare the variables
    uint32_t pathIndex = 0;
    uint32_t count = 0;
    char segmentName[256];
    uint32_t bytes_read = 0;
    uint32_t val = 0;


    while (1) {
        val = get_next_segment(path, &pathIndex, segmentName);

        // Check each path segment if it exsist and if yes find if the directory contain the next directory
        if (val == 0) {
            char name[11];
            uint32_t found = 0;
            uint32_t nextCluster = 0;

            lfn_state_t lfn_state;
            memset((unsigned char*)&lfn_state, 0, sizeof(lfn_state_t));

            if (count == 0) {
                uint32_t entries_per_sector = fat_bpb.bytes_per_sector / DIR_ENTRY_SIZE;
                for (uint32_t sector = root_dir_start_sector; sector < data_start_sector; sector++) {

                    char buffer[ATA_SECTOR_SIZE];

                    if (ata_read_sector(sector, (uint8_t*)buffer) != 0) return 1;

                    int32_t match_idx = findEntryInBuffer(segmentName, name, (dir_entry_t*)buffer, entries_per_sector, &lfn_state);

                    if (match_idx == -1) break;
                    if (match_idx >= 0) {

                        dir_entry_t* dir = &((dir_entry_t*)buffer)[match_idx];

                        if ((dir->attributes & 0x10) == 0) return 1;

                        found = 1;
                        nextCluster = dir->starting_cluster;

                        break;
                    }
                }
            }

            else {

                uint32_t total_entries = bytes_read / DIR_ENTRY_SIZE;
                int32_t match_index = findEntryInBuffer(segmentName, name, (dir_entry_t*)lastDirectoryBuffer, total_entries, &lfn_state);

                if (match_index >= 0) {
                    dir_entry_t* dir = &((dir_entry_t*)lastDirectoryBuffer)[match_index];
                    if ((dir->attributes & 0x10) == 0) return 1;

                    found = 1;
                    nextCluster = dir->starting_cluster;
                }
            }

            if (!found) return 1;

            uint32_t innerBytesRead = 0;
            uint32_t cluster_read = 0;

            // Prevent buffer overflow by capping read limit
            uint32_t bytes_to_read = DIRECTORY_SIZE;

            while (1) {

                if (cluster_read++ > 65536) return 1;

                // Prevent reading out of directory
                if (nextCluster < 2 || (nextCluster >= 0xFFF0 && nextCluster <= 0xFFF7)) return 1;
                if (nextCluster >= 0xFFF8) break;

                char sectorBuffer[ATA_SECTOR_SIZE];

                // Read the current cluster
                for (uint32_t t = 0; t < fat_bpb.sectors_per_cluster && innerBytesRead < bytes_to_read; t++) {
                    uint32_t data_sector = data_start_sector + (nextCluster - 2) * fat_bpb.sectors_per_cluster + t;
                    if (ata_read_sector(data_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                    uint32_t bytes_left_to_copy = bytes_to_read - innerBytesRead;
                    uint32_t chunk_size = bytes_left_to_copy < ATA_SECTOR_SIZE ? bytes_left_to_copy : ATA_SECTOR_SIZE;

                    memcpy(lastDirectoryBuffer + innerBytesRead, sectorBuffer, chunk_size);
                    innerBytesRead += chunk_size;
                }

                if (innerBytesRead >= bytes_to_read) break;

                uint32_t fat_sector = fat_start_sector + (2 * nextCluster) / ATA_SECTOR_SIZE;
                if (ata_read_sector(fat_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                uint16_t offset = (2 * nextCluster) % ATA_SECTOR_SIZE;
                uint8_t low_byte = (uint8_t)sectorBuffer[offset];
                uint8_t high_byte = (uint8_t)sectorBuffer[offset + 1];
                nextCluster = (uint16_t)(low_byte | (high_byte << 8));

            } // End of while loop
            bytes_read = innerBytesRead;
            count++;
        } // End of if

        else if (val == 1) {
            char name[11];
            uint32_t found = 0;
            uint32_t nextCluster = 0;

            uint32_t bytes_to_read = 0;
            lfn_state_t lfn_state;
            memset((unsigned char*)&lfn_state, 0, sizeof(lfn_state_t));

            if (count == 0) {
                uint32_t entries_per_sector = fat_bpb.bytes_per_sector / DIR_ENTRY_SIZE;
                for (uint32_t sector = root_dir_start_sector; sector < data_start_sector; sector++) {

                    char buffer[ATA_SECTOR_SIZE];

                    if (ata_read_sector(sector, (uint8_t*)buffer) != 0) return 1;

                    int32_t match_idx = findEntryInBuffer(segmentName, name, (dir_entry_t*)buffer, entries_per_sector, &lfn_state);

                    if (match_idx == -1) break;
                    if (match_idx >= 0) {

                        dir_entry_t* dir = &((dir_entry_t*)buffer)[match_idx];

                        if ((dir->attributes & 0x10) != 0) return 1;

                        found = 1;
                        nextCluster = dir->starting_cluster;

                        // Prevent buffer overflow by capping read limit
                        bytes_to_read = dir->file_size;

                        break;
                    }
                }
            }

            else {

                uint32_t total_entries = bytes_read / DIR_ENTRY_SIZE;
                int32_t match_index = findEntryInBuffer(segmentName, name, (dir_entry_t*)lastDirectoryBuffer, total_entries, &lfn_state);

                if (match_index >= 0) {
                    dir_entry_t* dir = &((dir_entry_t*)lastDirectoryBuffer)[match_index];
                    if ((dir->attributes & 0x10) != 0) return 1;

                    found = 1;
                    nextCluster = dir->starting_cluster;

                    // Prevent buffer overflow by capping read limit
                    bytes_to_read = dir->file_size;
                }
            }

            if (!found) return 1;

            uint32_t innerBytesRead = 0;
            uint32_t cluster_read = 0;

            uint32_t truncated = 0;
            if (bytes_to_read > maxBufferSize) {
                bytes_to_read = maxBufferSize;
                truncated = 1;
            }

            // If the file is empty
            if (bytes_to_read == 0) {
                *outFileSize = 0;
                return truncated ? 2 : 0; // 0 = success, 1 = error, 2 = success but truncated
            }

            while (1) {

                if (cluster_read++ > 65536) return 1;

                // Prevent reading out of directory
                if (nextCluster < 2 || (nextCluster >= 0xFFF0 && nextCluster <= 0xFFF7)) return 1;
                if (nextCluster >= 0xFFF8) break;

                char sectorBuffer[ATA_SECTOR_SIZE];

                // Read the current cluster
                for (uint32_t t = 0; t < fat_bpb.sectors_per_cluster && innerBytesRead < bytes_to_read; t++) {
                    uint32_t data_sector = data_start_sector + (nextCluster - 2) * fat_bpb.sectors_per_cluster + t;
                    if (ata_read_sector(data_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                    uint32_t bytes_left_to_copy = bytes_to_read - innerBytesRead;
                    uint32_t chunk_size = bytes_left_to_copy < ATA_SECTOR_SIZE ? bytes_left_to_copy : ATA_SECTOR_SIZE;

                    memcpy(fileBuffer + innerBytesRead, sectorBuffer, chunk_size);
                    innerBytesRead += chunk_size;
                }

                if (innerBytesRead >= bytes_to_read) break;

                uint32_t fat_sector = fat_start_sector + (2 * nextCluster) / ATA_SECTOR_SIZE;
                if (ata_read_sector(fat_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                uint16_t offset = (2 * nextCluster) % ATA_SECTOR_SIZE;
                uint8_t low_byte = (uint8_t)sectorBuffer[offset];
                uint8_t high_byte = (uint8_t)sectorBuffer[offset + 1];
                nextCluster = (uint16_t)(low_byte | (high_byte << 8));

            } // End of while loop
            bytes_read = innerBytesRead;
            count++;

            *outFileSize = bytes_read;
            return truncated ? 2 : 0; // 0 = success, 1 = error, 2 = success but truncated
        } // End of else if
        else if (val == 2) {
            break;
        }
    } // End of while 
    return 1;
} // End of fat_read_file




uint32_t listDirectory(char* path, char* directoryBuffer, uint32_t* outDirectorySize, uint32_t maxBufferSize) {

    static char lastDirectoryBuffer[DIRECTORY_SIZE];

    // Check if path and directoryBuffer exist
    if (!path || !directoryBuffer || !outDirectorySize) return 1;

    // Detect root
    uint32_t peek = 0;
    while (path[peek] == '/') peek++;
    uint32_t isRoot = (path[peek] == '\0');

    uint32_t bytes_read = 0;

    if (isRoot) {
        for (uint32_t sector = root_dir_start_sector; sector < data_start_sector; sector++) {
            if (bytes_read + ATA_SECTOR_SIZE > DIRECTORY_SIZE) break;
            if (ata_read_sector(sector, (uint8_t*)lastDirectoryBuffer + bytes_read)) return 1;
            bytes_read += ATA_SECTOR_SIZE;
        }
    }
    else {
        uint32_t pathIndex = 0;
        uint32_t count = 0;
        char segmentName[512];

        // Check each path segment if it exsist and if yes find if the directory contain the next directory
        while (get_next_segment(path, &pathIndex, segmentName) != 2) {
            char name[11];
            uint32_t found = 0;
            uint32_t nextCluster = 0;

            lfn_state_t lfn_state;
            memset((unsigned char*)&lfn_state, 0, sizeof(lfn_state_t));

            if (count == 0) {
                uint32_t entries_per_sector = fat_bpb.bytes_per_sector / DIR_ENTRY_SIZE;
                for (uint32_t sector = root_dir_start_sector; sector < data_start_sector; sector++) {

                    char buffer[ATA_SECTOR_SIZE];

                    if (ata_read_sector(sector, (uint8_t*)buffer) != 0) {
                        return 1;
                    }

                    int32_t match_idx = findEntryInBuffer(segmentName, name, (dir_entry_t*)buffer, entries_per_sector, &lfn_state);

                    if (match_idx == -1) break;
                    if (match_idx >= 0) {

                        dir_entry_t* dir = &((dir_entry_t*)buffer)[match_idx];

                        if ((dir->attributes & 0x10) == 0) return 1;

                        found = 1;
                        nextCluster = dir->starting_cluster;

                        break;
                    }
                }
            }
            else {

                uint32_t total_entries = bytes_read / DIR_ENTRY_SIZE;
                int32_t match_index = findEntryInBuffer(segmentName, name, (dir_entry_t*)lastDirectoryBuffer, total_entries, &lfn_state);

                if (match_index >= 0) {
                    dir_entry_t* dir = &((dir_entry_t*)lastDirectoryBuffer)[match_index];
                    if ((dir->attributes & 0x10) == 0) return 1;

                    found = 1;
                    nextCluster = dir->starting_cluster;
                }
            }

            if (!found) return 1;

            uint32_t innerBytesRead = 0;
            uint32_t cluster_read = 0;

            // Prevent buffer overflow by capping read limit
            uint32_t bytes_to_read = DIRECTORY_SIZE;

            while (1) {

                if (cluster_read++ > 65536) return 1;

                // Prevent reading out of directory
                if (nextCluster < 2 || (nextCluster >= 0xFFF0 && nextCluster <= 0xFFF7)) return 1;
                if (nextCluster >= 0xFFF8) break;

                char sectorBuffer[ATA_SECTOR_SIZE];

                // Read the current cluster
                for (uint32_t t = 0; t < fat_bpb.sectors_per_cluster && innerBytesRead < bytes_to_read; t++) {
                    uint32_t data_sector = data_start_sector + (nextCluster - 2) * fat_bpb.sectors_per_cluster + t;
                    if (ata_read_sector(data_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                    uint32_t bytes_left_to_copy = bytes_to_read - innerBytesRead;
                    uint32_t chunk_size = bytes_left_to_copy < ATA_SECTOR_SIZE ? bytes_left_to_copy : ATA_SECTOR_SIZE;

                    memcpy(lastDirectoryBuffer + innerBytesRead, sectorBuffer, chunk_size);
                    innerBytesRead += chunk_size;
                }

                if (innerBytesRead >= bytes_to_read) break;

                uint32_t fat_sector = fat_start_sector + (2 * nextCluster) / ATA_SECTOR_SIZE;
                if (ata_read_sector(fat_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                uint16_t offset = (2 * nextCluster) % ATA_SECTOR_SIZE;
                uint8_t low_byte = (uint8_t)sectorBuffer[offset];
                uint8_t high_byte = (uint8_t)sectorBuffer[offset + 1];
                nextCluster = (uint16_t)(low_byte | (high_byte << 8));

            } // End of while loop
            bytes_read = innerBytesRead;
            count++;
        } // End of while loop
    } // End of else if

    if (bytes_read > maxBufferSize) bytes_read = maxBufferSize;
    memcpy(directoryBuffer, lastDirectoryBuffer, bytes_read);
    *outDirectorySize = bytes_read;
    return 0;

} // End of listDirectory

void fat16_write_file(char* path, char* data, uint32_t rewriting) {

}
