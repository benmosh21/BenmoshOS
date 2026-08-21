
#endif


#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stddef.h>
#include "../src/drivers/ata/ata.h"      // Needed for ata_read_sector
#include "../src/drivers/print/print.h"  // Needed for your print function
#include "../src/memory/heap/heap.h"
#include "../src/kernel/system.h"

#pragma pack(push, 1)

typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_dir_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
} bpb_t;

typedef struct {
    uint8_t  filename[8];
    uint8_t  extension[3];
    uint8_t  attributes;
    uint8_t  reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t starting_cluster;
    uint32_t file_size;    
} dir_entry_t;

struct lfn_entry {
    uint8_t  sequence_number; 
    uint16_t name1[5];        
    uint8_t  attribute;       
    uint8_t  type;            
    uint8_t  checksum;        
    uint16_t name2[6];        
    uint16_t first_cluster;   
    uint16_t name3[2];        
} __attribute__((packed));

#pragma pack(pop)

typedef struct {
    char lfn_buffer[512];
    uint8_t checksum;
    uint8_t rightChecksum;
} lfn_state_t;

// Our main function that the kernel will call
void fat16_print_file(char* target_file);
uint8_t* fat16_load_file(char* target_file, uint32_t* out_file_size);
int read_files();
void fat16_create_file(char* target_name);
void fat16_write_file(char* target_file, char* data, uint32_t rewriting);
uint32_t fat16_init();

#endif

#define ATA_SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32
#define DIRECTORY_SIZE 4096

static bpb_t fat_bpb;
static uint32_t fat_start_sector;
static uint32_t root_dir_start_sector;
static uint32_t data_start_sector;


uint32_t get_next_segment(const char* path, uint32_t* pathIndex, char* outBuffer) {
    uint32_t counter = 0;
    
    // Skip any '/' in the start of the folder
    while (path[pathIndex[0]] == '/') {
        pathIndex[0]++;
    }

    if (path[pathIndex[0]] == '\0') {
        return 1;
    }

    // Add each char into the buffer untile null byter for end of path or / for end of directory name
    while (path[pathIndex[0] + counter] != '/' && path[pathIndex[0] + counter] != '\0') {
        // Prevent bounds overflow
        if (counter < 511) {
            outBuffer[counter] = path[pathIndex[0]+counter];
        }
        counter++;
    }

    // Add null terminator to the buffer
    outBuffer[counter < 511 ? counter : 511] = '\0';

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
            break;
        }

        if (entries[i].filename[0] == 0x05) {
            state->checksum = 0;
            state->rightChecksum = 0;
            memset((unsigned char*)state->lfn_buffer, 0, 512);
            continue;
        }

        if (entries[i].attributes == 0x0F) {
            struct lfn_entry* lfn = (struct lfn_entry*)&entries[i];

            int32_t chunkIndex = (lfn->sequence_number & 0x1F) - 1;

            // Safety bounds check for the chunk index
            if (chunkIndex >= 0 && chunkIndex <= 20) {
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
                    memset((unsigned char*)state->lfn_buffer, 0, 512);
                }
            }
        } // End of if

        // standard 8.3 SFN entry
        else {
            uint32_t lfnMatch = 0;
            uint32_t sfnMatch = 0;

            // Check if the LFN matched
            if (state->rightChecksum == 1 && strcmp(name, state->lfn_buffer) == 0) {
                uint8_t sfn_checksum = 0;
                for (uint32_t j = 0; j < 11; j++) {
                    sfn_checksum = ((sfn_checksum & 1) ? 0x80 : 0) + (sfn_checksum >> 1) + entries[i].filename[j];
                }
                if (state->checksum == sfn_checksum) {
                    lfnMatch = 1;
                }
            }
            
            // Check if the SFN matched
            char targetSFN[11];
            if (sfnEncoder(name, targetSFN) == 0) {
                if (memcmp(targetSFN, entries[i].filename, 11) == 0) {
                    sfnMatch = 1;
                }
            }

            // return success if either matched
            if (sfnMatch || lfnMatch) {
                memcpy((uint8_t*)formatName, (uint8_t*)entries[i].filename, 11);
                return 0;
            }
            // if not then we do the check again
            else {
                state->rightChecksum = 0;
                memset((unsigned char*)state->lfn_buffer, 0, 512);
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

    if (fat_bpb.sectors_per_cluster == 0 || fat_bpb.fat_count == 0 || fat_bpb.sectors_per_fat == 0) return 1; 

    // calculate the start of the fat & root_directory
    fat_start_sector = fat_bpb.reserved_sectors;
    root_dir_start_sector = fat_start_sector + (fat_bpb.fat_count * fat_bpb.sectors_per_fat);

    // Prevent truncation by safely rounding up
    uint32_t root_dir_bytes = fat_bpb.root_dir_entries * DIR_ENTRY_SIZE;
    uint32_t root_dir_sectors = (root_dir_bytes + fat_bpb.bytes_per_sector - 1) / fat_bpb.bytes_per_sector;

    data_start_sector = root_dir_start_sector + root_dir_sectors;

    return 0;
}


int fat_read_file(char* target_file, char* dest_buffer, uint32_t dest_buffer_size) {

    if (!target_file || !dest_buffer || dest_buffer_size == 0) return 1;

    char formatName[11];
    uint32_t entries_per_sector = fat_bpb.bytes_per_sector / DIR_ENTRY_SIZE;

    lfn_state_t lfn_state;
    memset((unsigned char*)&lfn_state, 0, sizeof(lfn_state_t));

    for (uint32_t sector = root_dir_start_sector; sector < data_start_sector; sector++) {
        char buffer[ATA_SECTOR_SIZE];

        if (ata_read_sector(sector, (uint8_t*)buffer) != 0) return 1;
        dir_entry_t* entries = (dir_entry_t*)buffer;

        int32_t match_idx = findEntryInBuffer(target_file, formatName, entries, entries_per_sector, &lfn_state);

        if (match_idx == -1) return 1;
        if (match_idx >= 0) {

            if ((entries[match_idx].attributes & 0x10) != 0) return 1; // It's a directory

            uint32_t nextCluster = entries[match_idx].starting_cluster;
            uint32_t bytes_read = 0;

            // Prevent buffer overflow by capping read limit
            uint32_t bytes_to_read = entries[match_idx].file_size;

            if (bytes_to_read > dest_buffer_size) bytes_to_read = dest_buffer_size;

            uint32_t cluster_read = 0;
            while (bytes_read < bytes_to_read) {

                if (cluster_read++ > 65536) return 1;

                // Prevent underflow on corrupted starting_cluster
                if (nextCluster < 2 || (nextCluster >= 0xFFF0 && nextCluster <= 0xFFF7)) {
                    return 1; // Error: Corrupted FAT chain or Bad Cluster encountered
                }

                if (nextCluster >= 0xFFF8) {
                    if (bytes_read < bytes_to_read) {
                        return 1; // Error: Truncated file, missing clusters
                    }
                    break; // Normal EOF (though we usually break before this due to bytesRead)
                }

                char sectorBuffer[ATA_SECTOR_SIZE];

                // Read the current cluster
                for (uint32_t t = 0; t < fat_bpb.sectors_per_cluster && bytes_read < bytes_to_read; t++) {
                    uint32_t data_sector = data_start_sector + (nextCluster - 2) * fat_bpb.sectors_per_cluster + t;

                    if (ata_read_sector(data_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                    uint32_t bytes_left = bytes_to_read - bytes_read;
                    uint32_t chunk_size = bytes_left < ATA_SECTOR_SIZE ? bytes_left : ATA_SECTOR_SIZE;

                    memcpy((unsigned char*)dest_buffer + bytes_read, (uint8_t*)sectorBuffer, chunk_size);

                    bytes_read += chunk_size;
                }

                if (bytes_read >= bytes_to_read) { break; }

                // Find the next cluster
                uint32_t max_fat_entries = (fat_bpb.sectors_per_fat * ATA_SECTOR_SIZE) / 2;
                if (nextCluster >= max_fat_entries) return 1;

                uint32_t fat_sector = fat_start_sector + (2 * nextCluster) / ATA_SECTOR_SIZE;

                if (ata_read_sector(fat_sector, (uint8_t*)sectorBuffer) != 0) {
                    return 1;
                }
                uint16_t offset = (2 * nextCluster) % ATA_SECTOR_SIZE;
                uint8_t low_byte = (uint8_t)sectorBuffer[offset];
                uint8_t high_byte = (uint8_t)sectorBuffer[offset + 1];
                nextCluster = (uint16_t)(low_byte | (high_byte << 8));

            } // End of while loop

            return 0; // Success

        } // End of if

    } // End of for loop

    return 1; // File not found

} // End of fat_read_file


uint32_t listDirectory(char* path, char* directoryBuffer, uint32_t* outDirectorySize) {

    static char lastDirectoryBuffer[DIRECTORY_SIZE];

    // Check if path and directoryBuffer exist
    if (!path || !directoryBuffer) return 1;

    // Declare the variables
    uint32_t pathIndex = 0;
    uint32_t count = 0;
    char segmentName[512];
    uint32_t bytes_read = 0;

    // Check each path segment if it exsist and if yes find if the directory contain the next directory
    while (get_next_segment(path, &pathIndex, segmentName) == 0) {
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

    memcpy(directoryBuffer, lastDirectoryBuffer, bytes_read);
    *outDirectorySize = bytes_read;
    return 0;

} // End of listDirectory

void fat16_wrtie_file(char* target_file, char* data, uint32_t rewriting) {
    if()
}