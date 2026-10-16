#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stddef.h>
#include "src/drivers/ata/ata.h"
#include "src/klib/string.h"

#pragma pack(push, 1)

// Maps the first 36 bytes of Sector 0
typedef struct {
    uint8_t  jump[3];               // Jump instruction to boot code (e.g., 0xEB, 0x3C, 0x90)
    uint8_t  oem[8];                // OEM identifier (e.g., "BMOS    ")
    uint16_t bytes_per_sector;      // 512
    uint8_t  sectors_per_cluster;   // 1
    uint16_t reserved_sectors;      // 50
    uint8_t  fat_count;             // 2
    uint16_t root_dir_entries;      // 512
    uint16_t total_sectors_16;      // 0
    uint8_t  media_descriptor;      // 0xF8
    uint16_t sectors_per_fat;       // 256
} bpb_t;

// Maps the 32-byte FAT16 Directory Entry
typedef struct {
    uint8_t  filename[8];       // 8.3 filename (padded with spaces)
    uint8_t  extension[3];      // 8.3 extension (padded with spaces)
    uint8_t  attributes;        // File attributes (e.g., read-only, hidden, system, volume label, directory, archive)
    uint8_t  reserved[10];      // Reserved for future use (=0)
    uint16_t time;              // Time of last modification (in FAT time format)
    uint16_t date;              // Date of last modification (in FAT date format)
    uint16_t starting_cluster;  // Starting cluster number of the file/directory
    uint32_t file_size;         // Size of the file in bytes (0 for directories)
} dir_entry_t;

struct lfn_entry {
    uint8_t  sequence_number; // Order of this entry (1, 2, 3...)
    uint16_t name1[5];        // First 5 characters (UTF-16)
    uint8_t  attribute;       // MUST be 0x0F to prove it's a ghost entry
    uint8_t  type;            // Always 0x00
    uint8_t  checksum;        // Matches this ghost to the real 8.3 file below it
    uint16_t name2[6];        // Next 6 characters (UTF-16)
    uint16_t first_cluster;   // Always 0x0000 in a ghost entry
    uint16_t name3[2];        // Last 2 characters (UTF-16)
} __attribute__((packed));


#pragma pack(pop)


// Filesystem API
void     fat16_init(void);
void     fat16_list_files(void);
void     fat16_print_file(const char* target_file);
uint8_t* fat16_load_file(const char* target_file, uint32_t* out_file_size);
int      fat16_create_file(const char* target_name);
int      fat16_write_file(const char* target_file, const char* data, int append);
int      fat16_delete_file(const char* target_file);


#endif
