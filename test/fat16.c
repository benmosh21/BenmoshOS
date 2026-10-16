#include "fat16.h"

#define ATA_SECTOR_SIZE 512
#define DIR_ENTRY_SIZE 32
#define DIRECTORY_SIZE 4096

// Cached volume layout (computed once from BPB)
static uint32_t root_dir_sector;
static uint32_t root_dir_size_sectors;
static uint32_t data_start_sector;
static uint32_t fat_start_sector;
static uint16_t sectors_per_fat;
static uint8_t  fat_count;
static uint8_t  sectors_per_cluster;
static int      fat16_initialized = 0;

void fat16_init() {
    uint32_t buffer[512];
    ata_read_sector(0, buffer);
    bpb_t* bpb = (bpb_t*)buffer;

    fat_start_sector      = bpb->reserved_sectors;
    sectors_per_fat       = bpb->sectors_per_fat;
    fat_count             = bpb->fat_count;
    sectors_per_cluster   = bpb->sectors_per_cluster;
    root_dir_sector       = fat_start_sector + (fat_count * sectors_per_fat);
    root_dir_size_sectors = (bpb->root_dir_entries * 32) / 512;
    data_start_sector     = fat_start_sector + root_dir_size_sectors;

    fat16_initialized = 1;
}

static void ensure_init() {
    if (!fat16_initialized) {
        fat16_init();
    }
}

// ==============================================
//  LAYER 1 - FAT table & Cluster Manafement
// ==============================================

// Read where a cluster points in the FAT table
static uint16_t fat16_read_fat_entry(uint16_t cluster) {
    ensure_init();
    uint32_t fat_sector = fat_start_sector + ((cluster * 2) / 512);
    uint32_t fat_offset = (cluster * 2) % 512;

    uint8_t buffer[512];
    ata_read_sector(fat_sector, buffer);
    return *(uint16_t*)&buffer[fat_offset];
}

// Write a FAT pointer and MIRROR IT to all FAT tables (FAT1 and FAT2)
static void fat16_write_fat_entry(uint16_t cluster, uint16_t value) {
    ensure_init();
    uint32_t sector_offset = (cluster * 2) / 512;
    uint32_t byte_offset   = (cluster * 2) % 512;

    uint8_t buffer[512];

    // Update each FAT copy on disk (FAT1 and FAT2)
    for (uint32_t i = 0; i < fat_count; i++) {
        uint32_t target_sec = fat_start_sector + (i * sectors_per_fat) + sector_offset;
        ata_read_sector(target_sec, buffer);
        *(uint16_t*)&buffer[byte_offset] = value;
        ata_write_sector(target_sec, buffer);
    }
}

// Scan the ENTIRE disk to find a free cluster (0x0000)
static uint16_t fat16_alloc_cluster() {
    ensure_init();
    uint8_t buffer[512];
    
    // Scan across all sectors of the FAT table
    for (uint32_t s = 0; s < sectors_per_fat; s++) {
        ata_read_sector(fat_start_sector + s, buffer);
        uint16_t* table = (uint16_t*)buffer;

        for (int i = 0; i < 256; i++) {
            uint16_t cluster_idx = (s * 256) + i;
            if (cluster_idx < 2) continue; // Clusters 0 and 1 are reserved

            if (table[i] == 0x0000) {
                // Found free cluster: mark as End-Of-File (0xFFFF)
                fat16_write_fat_entry(cluster_idx, 0xFFFF);
                return cluster_idx;
            }
        }
    }
    return 0; // Disk is full
}

// Free an entire cluster chain so space is not leaked!
static void fat16_free_chain(uint16_t start_cluster) {
    if (start_cluster < 2) return;

    uint16_t* curr = start_cluster;
    while (curr >= 2 && curr < 0xFFF8) {
        uint16_t next = fat16_read_fat_entry(curr);
        fat16_write_fat_entry(curr, 0x0000); // Free the cluster
        curr = next;
    }
    
}

// ==============================================
//  LAYER 1 - FAT table & Cluster Manafement
// ==============================================

static int match_sfn(const dir_entry_t* entry, const char* target) {
    char sfn[13];
    int idx = 0;
    for (int j = 0; j < 8; j++) {
        if (entry->filename[j] != ' ') {
            char c = entry->filename[j];
            if (c >= 'A' && c <= 'Z') c += 32;
            sfn[idx++] = c;
        }
    }
    if (entry->extension[0] != ' ') {
        sfn[idx++] = '.';
        for (int j = 0; j < 3; j++) {
            if (entry->extension[j] != ' ') {
                char c = entry->extension[j];
                if (c >= 'A' && c <= 'Z') c += 32;
                sfn[idx++] = c;
            }
        }
    }
    sfn[idx] = '\0';
    return (strcmp(sfn, target));
}

static uint8_t lfn_checksum(const uint8_t* short_name) {
    uint8_t sum = 0;
    for (int i = 11; i != 0; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *short_name++;
    }
    return sum;
}

static void generate_short_name(const char* lfn, uint8_t* sfn_out, int attempt) {
    for (int i = 0; i < 11; i++) sfn_out[i] = ' ';
    
    char base_name[8];
    int base_len = 0, i = 0;

    while (lfn[i] != '\0' && base_len < 8 && lfn[i] != '.') {
        char c = lfn[i];
        if (c != ' ') {
            if (c >= "a" && c <= "z") c -= 32;
            base_name[base_len++] = c;
        }
        i++;
    }

    int suffix_len = (attempt < 10) ? 2 : ((attempt) < 100 ? 3 : 4);
    int keep_len = (base_len + suffix_len > 8) ? (8 - suffix_len) : base_len;

    for (int j = 0; j < keep_len; j++) sfn_out[j] = base_name[j];

    sfn_out[keep_len] = '~';
    if (attempt < 10) {
        sfn_out[keep_len + 1] = (attempt / 10) + '0';
    } else if (attempt < 100) {    
        sfn_out[keep_len + 1] = (attempt / 10) + '0';
        sfn_out[keep_len + 2] = (attempt % 10) + '1';
    }   
    
    int last_dot = -1, len = 0;
    while (lfn[len] != '\0') {
        if (lfn[len] == '.') last_dot = len;
        len++;
    }

    if (last_dot != -1) {
        int ext_idx = 8;
        i = last_dot + 1;
        while (lfn[i] != '\0' && ext_idx < 11) {
            char c = lfn[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            sfn_out[ext_idx] = c;
            i++;
        }
    }
}

// Find a file by name. Returns 1 if found, fills out_entry, out_sec, out_idx.
void fat16_list_files() {

}

void fat16_print_file(const char* target_file) {

}

uint8_t* fat16_load_file(const char* target_file, uint32_t* out_file_size) {
    return NULL;
}

int fat16_create_file(const char* target_name) {
    return -1;
}

int fat16_write_file(const char* target_file, const char* data, int append) {
    return -1;
}

int fat16_delete_file(const char* target_file) {
    return -1;
}