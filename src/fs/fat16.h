#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stddef.h>
#include "../drivers/print/print.h"
#include "../drivers/ata/ata.h"
#include "../memory/heap/heap.h"
#include "../klib/string.h"

/* Standard Status & Error Codes */
#define EOK          0   /* Success */
#define ENOENT       2   /* No such file or directory */
#define EIO          5   /* Hardware disk I/O error */
#define ENOMEM      12   /* Out of memory */
#define EEXIST      17   /* File or directory already exists */
#define ENOTDIR     20   /* Component of path is not a directory */
#define EISDIR      21   /* Is a directory, operation expected a file */
#define EINVAL      22   /* Invalid argument */
#define ENOSPC      28   /* No space left on disk */
#define ENOTEMPTY   39   /* Directory not empty */

/* FAT Attributes */
#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LFN       0x0F

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
};

#pragma pack(pop)

/* Public API */
void     fat16_init(void);
int      fat16_list_directory(const char* path);
uint8_t* fat16_load_file(const char* path, uint32_t* out_file_size);
int      fat16_create_file(const char* path);
int      fat16_write_file(const char* path, const char* data, uint32_t len, int append);
int      fat16_delete_file(const char* path);
int      fat16_make_directory(const char* path);
int      fat16_remove_directory(const char* path);

#endif /* FAT16_H */