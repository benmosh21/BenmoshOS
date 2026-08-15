
#endif


#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stddef.h>
#include "../src/drivers/ata/ata.h"      // Needed for ata_read_sector
#include "../src/drivers/print/print.h"  // Needed for your print function
#include "../src/memory/heap/heap.h"

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
#pragma pack(pop)


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

// Our main function that the kernel will call
void fat16_print_file(char* target_file);
uint8_t* fat16_load_file(char* target_file, uint32_t* out_file_size);
int read_files();
void fat16_create_file(char* target_name);
void fat16_write_file(char* target_file, char* data, int rewriting);
int fat16_init();

#endif

#define ATA_SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32

bpb_t fat_bpb;

uint32_t fat_start_sector;
uint32_t root_dir_start_sector;
uint32_t data_start_sector;

int phrezePath(char* path, char** pathSegment) {
    if (path == NULL || pathSegment == NULL || pathSegment == '\0') {
        return 0;
    }

    while (**)
}

// Returns 0 on success, 1 if the name is invalid for an 8.3 format
int sfnEncoder(char* name, char* sfn) {
    // A standard 8.3 name has a strict maximum of 12 characters:
    // 8 for the base name + 1 dot + 3 for the extension.
    if (strlen(name) > 12) {
        return 1;
    }

    // Pre-fill the 11-byte output array entirely with spaces.
    // This automatically handles files that don't have extensions!
    memset(sfn, ' ', 11);

    int r = 0; // Read index to track where we are in 'name'
    int w = 0; // Write index to track where we are in 'sfn'
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

int getFormatName(char* name, char* formatName) {
    // We need a temporary buffer OUTSIDE the sector loops to hold the fragmented name
    char lfn_buffer[256];
    memset(lfn_buffer, 0, 256);

    uint8_t checksum = 0;
    uint8_t rightChecksum = 0;
    uint32_t entries_per_sector = ATA_SECTOR_SIZE / DIR_ENTRY_SIZE;

    for (uint32_t sector = root_dir_start_sector; sector < data_start_sector; sector++) {
        char buffer[ATA_SECTOR_SIZE];
        if (ata_read_sector(sector, buffer) != 0) return 1;

        dir_entry_t* entries = (dir_entry_t*)buffer;
        
        for (int i = 0; i < entries_per_sector; i++) {

            // Check if there is more files
            if ((uint32_t)entries[i].filename[0] == 0x00) {
                return 1;
            }
            // Check if its a deleted file
            else if ((uint32_t)entries[i].filename[0] == 0xE5) {
                checksum = 0;
                rightChecksum = 0;
                memset(lfn_buffer, 0, 256);
                continue;
            }
            // handle LFN ghost entrie
            else if (entries[i].attributes == 0x0F) {
                
                //  cast the entrie as a ghost entry
                struct lfn_entry* lfn = (struct lfn_entry*)&entries[i];
                
                // Strip the 0x40 flag and calculate the starting index
                int chunk_index = (lfn->sequence_number & 0x1F) - 1;
                int offset = chunk_index * 13; 

                // skipping the 0x0000 terminator and 0xFFFF padding
                for (int j = 0; j < 5; j++) {
                    if (lfn->name1[j] != 0xFFFF && lfn->name1[j] != 0x0000) {
                        lfn_buffer[offset++] = (char)(lfn->name1[j] & 0xFF);
                    }
                }
                for (int j = 0; j < 6; j++) {
                    if (lfn->name2[j] != 0xFFFF && lfn->name2[j] != 0x0000) {
                        lfn_buffer[offset++] = (char)(lfn->name2[j] & 0xFF);
                    }
                }
                for (int j = 0; j < 2; j++) {
                    if (lfn->name3[j] != 0xFFFF && lfn->name3[j] != 0x0000) {
                        lfn_buffer[offset++] = (char)(lfn->name3[j] & 0xFF);
                    }
                }

                // Verify that all ghost entries in this stack share the exact same checksum
                if (rightChecksum == 0) {
                    checksum = lfn->checksum;
                    rightChecksum = 1;
                } 
                else if (rightChecksum == 1) {
                    if (checksum != lfn->checksum) {
                        rightChecksum = 2; // Checksum corrupted, mark as invalid
                        memset(lfn_buffer, 0, 256);
                    }
                }
                
            }
            // standard 8.3 SFN entry
            else {
                int lfnMatch = 0;
                int sfnMatch = 0;

                // if the name is LFN and the checksums matches
                if (rightChecksum == 1 && strcmp(name, lfn_buffer) == 0) {
                    uint8_t sfn_checksum = 0;
                    for (int j = 0; j < 11; j++) {
                        sfn_checksum = ((sfn_checksum & 1) ? 0x80 : 0) + (sfn_checksum >> 1) + entries[i].filename[j];
                    }
                    if (checksum == sfn_checksum) {
                        lfnMatch = 1;
                    }
                }
                // if the name is SFN and mached the SFN name
                char targetSFN[11];
                // Check memory only if the encoder returned success
                if (sfnEncoder(name, targetSFN) == 0) {
                    if (memcmp(targetSFN, entries[i].filename, 11) == 0) {
                        sfnMatch = 1;
                    }
                }
                // if there is a match its mean this is the right entries.
                if (sfnMatch || lfnMatch) {
                    memcpy(formatName, entries[i].filename, 11);
                    return 0;
                }
                // if not then we do the check again
                else {
                    rightChecksum = 0;
                    memset(lfn_buffer, 0, 256);
                    continue;
                }
            }
        }
    }
    return 1; // the file name wasnt fount in the dir entries.
}

int fat16_init() {
    char buffer[ATA_SECTOR_SIZE];
    
    // Check for read errors
    if (ata_read_sector(0, buffer) != 0) {
        return 1;
    }
    
    memcpy(&fat_bpb, buffer, sizeof(fat_bpb));
    
    // Ensure FAT format uses 512-byte sectors to match our ATA driver
    if (fat_bpb.bytes_per_sector != ATA_SECTOR_SIZE) {
        return 1;
    }

    if (fat_bpb.sectors_per_cluster == 0 || fat_bpb.fat_count == 0 || fat_bpb.sectors_per_fat == 0) {
        return 1; 
    }

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

    if (!target_file || !dest_buffer || dest_buffer_size == 0) {
        return 1;
    }

    char formatName[11];
    if (getFormatName(target_file, formatName)) {
        return 1;
    }

    if (!formatName) {
        return 1;
    }

    uint32_t entries_per_sector = fat_bpb.bytes_per_sector / DIR_ENTRY_SIZE;

    for (uint32_t sector = root_dir_start_sector; sector < data_start_sector; sector++) {
        char buffer[ATA_SECTOR_SIZE];

        if (ata_read_sector(sector, buffer) != 0) {
            return 1;
        }

        dir_entry_t* entries = (dir_entry_t*)buffer;

        for (int i =0; i < entries_per_sector; i++) {
            if ((uint32_t)entries[i].filename[0] == 0x00) {
                return 1; // End of root directory
            }
            
            if ((uint8_t)entries[i].filename[0] == 0xE5) {
                continue; // Deleted file
            }

            if ((entries[i].attribute & 0x18) != 0) {
                continue; // directories, volume lables and LFNs
            }

            if (memcmp(entries[i].filename, formatName, 11) == 0) {
                uint32_t nextCluster = entries[i].starting_cluster;
                uint32_t bytes_read = 0;

                // Prevent buffer overflow by capping read limit
                uint32_t bytes_to_read = entries[i].file_size;
                if (bytes_to_read > dest_buffer_size) {
                    bytes_to_read = dest_buffer_size;
                }

                uint32_t cluster_read = 0;
                while (bytes_read < bytes_to_read )  {

                    if (cluster_read++ > 65536) {
                        return 1;
                    }

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
                    for (int t = 0; t < fat_bpb.sectors_per_cluster && bytes_read < bytes_to_read; t++) {
                        uint32_t data_sector = data_start_sector + (nextCluster-2) * fat_bpb.sectors_per_cluster + t;

                        if (ata_read_sector(data_sector, sectorBuffer) != 0) {
                            return 1;
                        }

                        uint32_t bytes_left_to_copy = bytes_to_read - bytes_read;
                        uint32_t chunk_size = bytes_left_to_copy < ATA_SECTOR_SIZE ? bytes_left_to_copy : ATA_SECTOR_SIZE;

                        memcpy(dest_buffer + bytes_read, sectorBuffer, chunk_size);

                        bytes_read += chunk_size;
                    }
                    
                    if (bytes_read >= bytes_to_read) { break; }

                    // Find the next cluster
                    uint32_t max_fat_entries = (fat_bpb.sectors_per_fat * ATA_SECTOR_SIZE) / 2;
                    if (nextCluster >= max_fat_entries) {
                        // Free formatName if dynamically allocated
                        return 1; 
                    }

                    uint32_t fat_sector = fat_start_sector + (2*nextCluster)/ATA_SECTOR_SIZE;

                    if (ata_read_sector(fat_sector, sectorBuffer) != 0) {
                        return 1;
                    }
                    uint16_t offset = (2 * nextCluster) % ATA_SECTOR_SIZE;
                    uint8_t low_byte = (uint8_t)sectorBuffer[offset];
                    uint8_t high_byte = (uint8_t)sectorBuffer[offset + 1];
                    nextCluster = (uint16_t)(low_byte | (high_byte << 8));
                }
                return 0; // Success
            }
        }
    }
    return 1; // File not found
}

void fat16_wrtie_file(char* target_file, char* data, int rewriting) {
    if()
}