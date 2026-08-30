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
    union {
        struct {
            uint8_t  filename[8];
            uint8_t  extension[3];
        };
        uint8_t short_name[11];
    };
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
    char lfn_buffer[261];
    uint8_t checksum;
    uint8_t rightChecksum;
} lfn_state_t;

typedef struct {
    uint32_t start_cluster;
    uint32_t current_cluster;
    uint32_t position;      // byte offset within file
    uint32_t file_size;
    uint8_t  is_directory;
} fat16_handle_t;


// Our main function that the kernel will call
uint32_t listDirectory(char* path, char* directoryBuffer, uint32_t* outDirectorySize, uint32_t maxBufferSize);
uint32_t fat_read_fileT(char* path, char* fileBuffer, uint32_t maxBufferSize, uint32_t* outFileSize);
uint32_t fat16_init();
void fat16_create_file(char* target_name); // --
void fat16_write_file(char* target_file, char* data, uint32_t rewriting);
uint32_t fat16_init();


int32_t fat16_open(const char* path, fat16_handle_t out_handle);
int32_t fat16_read(fat16_handle_t* handle, void* buffer, uint32_t bytesToRead, uint32_t* bytes_read);
int32_t fat16_seek(fat16_handle_t* handle, uint32_t offset);
int32_t fat16_close(fat16_handle_t* handle);

#endif


#define ATA_SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32
#define DIRECTORY_SIZE 4096

static bpb_t fat_bpb;
static uint32_t fat_start_sector;
static uint32_t root_dir_start_sector;
static uint32_t data_start_sector;

int32_t fat16_open(const char* path, fat16_handle_t out_handle) {

    // Check if path and fileBuffer exist
    if (!path || !out_handle) return 1;

    char directoryBuffer[DIRECTORY_SIZE];

    uint32_t pathIndex = 0;
    uint32_t count = 0;
    char segmentName[256];
    uint32_t bytes_read = 0;
    uint32_t val = 0;


    while (1) {
        // get next segment + get the val
        val = get_next_segment(path, &pathIndex, segmentName);

        if (val == 2) return 1; // got only EOF and no data

        char name[11];
        uint32_t found = 0;
        uint32_t nextCluster = 0;
        uint32_t attributes = 0;
        uint32_t file_size = 0;

        lfn_state_t lfn_state;
        memset((unsigned char*)&lfn_state, 0, sizeof(lfn_state_t));


        // find the segment dir_entry_t
        if (count == 0) {
            // search the ROOT directory via ATA, sector by sector
            uint32_t entries_per_sector = fat_bpb.bytes_per_sector / DIR_ENTRY_SIZE;
            for (uint32_t sector = fat_start_sector; sector < data_start_sector; sector++) {
                char sectorBuf[ATA_SECTOR_SIZE];
                ata_read_sector(sector, sectorBuf);

                // find next entry index
                int32_t matchIdx = findEntryInBuffer(segmentName, name, (dir_entry_t*)sectorBuf, entries_per_sector, &lfn_state);
                if (matchIdx == -1) break;
                if (matchIdx >= 0) {
                    // find the corrent segment out_handle value
                    dir_entry_t* dir = &((dir_entry_t)sectorBuf)[matchIdx];
                    found = 1;
                    nextCluster = dir->starting_cluster;
                    file_size = dir->file_size;
                    attributes = dir->attributes;
                    break;
                }
            }
        }
        else {
            // search the PREVIOUSLY loaded directoryBuffer (already in memory, no ATA needed)
            uint32_t total_entires = bytes_read / DIR_ENTRY_SIZE;
            int32_t matchIdx = findEntryInBuffer(segmentName, name, (dir_entry_t*)directoryBuffer, total_entires, &lfn_state);

            if (matchIdx >= 0) {
                // find the corrent segment out_handle value
                dir_entry_t* dir = &((dir_entry_t)directoryBuffer)[matchIdx];
                found = 1;
                nextCluster = dir->starting_cluster;
                file_size = dir->file_size;
                attributes = dir->attributes;
            }
        }

        (!found) return 1;

        // if we found the final file we can set the out_handle data
        if (val == 1) {
            out_handle.current_cluster  = nextCluster;
            out_handle.start_cluster    = nextCluster;
            out_handle.position         = 0;
            out_handle.file_size        = file_size;
            out_handle.is_directory     = (attributes & 0x10) != 0;
            return 0; // 
        }
        
        // if it a file when it in the middle of the path
        if ((attributes & 0x10) == 0) return 1; 

        // read the subdirectory to find the data and next loop read from the directory
        uint32_t innerBytesRead = 0;
        uint32_t cluster_read = 0;
        uint32_t bytes_to_read = DIRECTORY_SIZE;

        while (1) {
            // prevend overflow bugs
            if (cluster_read++ > 65536) return 1;
            if (nextCluster < 2 || (nextCluster >= 0xFFF0 && nextCluster <= 0xFFF7)) return 1;
            if (nextCluster >= 0xFFF8) break;

            // read the directory data and store it to find the next segmen in it
            char sectorBuffer[ATA_SECTOR_SIZE];
            for (uint32_t t = 0; t < fat_bpb.sectors_per_cluster && innerBytesRead < bytes_to_read; t++) {
                uint32_t data_sector = data_start_sector + (nextCluster - 2) * fat_bpb.sectors_per_cluster + t;
                if (ata_read_sector(data_sector, (uint8_t*)sectorBuffer) != 0) return 1;

                uint32_t bytes_left = bytes_to_read - innerBytesRead;
                uint32_t chunk = bytes_left < ATA_SECTOR_SIZE ? bytes_left : ATA_SECTOR_SIZE;
                memcpy(directoryBuffer + innerBytesRead, sectorBuffer, chunk);
                innerBytesRead += chunk;
            }

            if (innerBytesRead >= bytes_to_read) break;

            // find next cluster by readin the FAT from nextCluster and calculating how many bytes i read
            uint32_t fat_sector = fat_sector_start + (2 * nextCluster) / ATA_SECTOR_SIZE;
            if (ata_read_sector(fat_sector, (uint8_t*)sectorBuffer) != 0) return 1;
            uint16_t offset = (2 * nextCluster) % ATA_SECTOR_SIZE;
            nextCluster = (uint16)((uint8_t)sectorBuffer[offset]) | ((uint8_t)sectorBuffer[offset + 1] << 8));
        }

        bytes_read = innerBytesRead;
        count++;
    } // End of while
    return 1;
}


int32_t fat16_read(fat16_handle_t* handle, void* buffer, uint32_t bytesToRead, uint32_t* bytes_read) {
    if (!handle || !buffer || !bytes_read || bytesToRead == 0) return 1;

    uint32_t innerBytesRead = 0;
    uint32_t cluster_read = 0;
    uint32_t nextCluster = handle->current_cluster;

    if (handle->file_size < handle->position + bytesToRead) return 1;

    while (1) {
        if (cluster_read++ > 65536) return 1;
        if (nextCluster < 2 || (nextCluster >= 0xFFF0 && nextCluster <= 0xFFF7)) return 1;
        if (nextCluster >= 0xFFF8) break;
        if (innerBytesRead > bytesToRead) return 1;

        // read the directory data and store it to find the next segmen in it
        char sectorBuffer[ATA_SECTOR_SIZE];
        for (uint32_t t = 0; t < fat_bpb.sectors_per_cluster && innerBytesRead < bytes_to_read; t++) {
            uint32_t data_sector = data_start_sector + (nextCluster - 2) * fat_bpb.sectors_per_cluster + t;
            uint32_t offset = handle->position % (fat_bpb.bytes_per_sector * fat_bpb.sectors_per_cluster);
            if (ata_read_sector(data_sector, (uint8_t*)sectorBuffer) != 0) return 1;

            uint32_t bytes_left = bytes_to_read - innerBytesRead;
            uint32_t chunk = bytes_left < ATA_SECTOR_SIZE ? bytes_left : ATA_SECTOR_SIZE;
            memcpy(buffer + innerBytesRead, sectorBuffer + offset, chunk);
            innerBytesRead += chunk;
        }

        // find the next cluster
        uint32_t fat_sector = fat_start_sector + (2 * nextCluster) / ATA_SECTOR_SIZE;
        if (ata_read_sector(fat_sector, (uint8_t*)sectorBuffer) != 0) return 1;
        uint16_t fatOffset = (2 * nextCluster) % ATA_SECTOR_SIZE;
        nextCluster = (uint16_t)((uint8_t)sectorBuffer[fatOffset]) | ((uint8_t)sectorBuffer[fatOffset + 1] << 8)
    }

    *bytes_read = innerBytesRead;
    handle->position += innerBytesRead;
    handle->current_cluster = nextCluster;

    return 0;
}


int32_t fat16_seek(fat16_handle_t* handle, uint32_t offset) {
    if (!handle) return 1;
    if (offset > handle->file_size) return 1;

    uint32_t nextCluster = 0;
    uint32_t clusterOffset = 0;

    if (offset < handle->position) {
        // Can't go backward in a singly-linked chain — restart from the top
        nextCluster = handle->start_cluster;
        clusterOffset = (offset / (fat_bpb.bytes_per_sector * fat_bpb.sectors_per_cluster));
    }
    else {
        nextCluster = handle->current_cluster;
        clusterOffset = (offset / (fat_bpb.bytes_per_sector * fat_bpb.sectors_per_cluster)) - (handle->position / (fat_bpb.bytes_per_sector * fat_bpb.sectors_per_cluster));
    }

    char sectorBuffer[ATA_SECTOR_SIZE];

    for (uint32_t i = 0; i < clusterOffset; i++) {
        uint32_t fat_sector = fat_start_sector + (2 * nextCluster) / ATA_SECTOR_SIZE;
        if (ata_read_sector(fat_sector, (uint8_t*)sectorBuffer) != 0) return 1;
        uint16_t fatOffset = (2 * nextCluster) % ATA_SECTOR_SIZE;
        nextCluster = (uint16_t)((uint8_t)sectorBuffer[fatOffset]) | ((uint8_t)sectorBuffer[fatOffset + 1] << 8);
    }
    handle->current_cluster = nextCluster;
    handle->position = offset;

    return 0;
}


int32_t fat16_close(fat16_handle_t* handle) {
    return 1;
}

