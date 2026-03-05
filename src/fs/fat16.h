#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include "../drivers/print/print.h"

#pragma pack(push, 1)
// Maps the first 36 bytes of Sector 0
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

// Maps the 32-byte FAT16 Directory Entry
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

// Tell the compiler NOT to add secret padding bytes to this struct
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

// Our main function that the kernel will call
void fat16_print_file(char* target_file);
void fat16_list_files();
void fat16_create_file(char* target_name);
void fat16_write_file(char* target_file, char* data, int rewriting);

#endif