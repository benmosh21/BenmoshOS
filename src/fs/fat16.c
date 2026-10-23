/**
 * @file fat16.c
 * @brief Full-Featured FAT16 Filesystem Driver
 *
 * Implements a complete FAT16 filesystem layer with hierarchical directory
 * support, path resolution, Long File Name (LFN) creation/parsing, FAT chain
 * allocation, read/write/append file operations, and directory management (mkdir/rmdir).
 *
 * Architectural Design:
 *  - Layer 1: Sector-level FAT entry read/write, allocation, and chain traversal.
 *  - Layer 2: 8.3 SFN generation, LFN checksumming, and filename normalization.
 *  - Layer 3: Unified directory scanning engine using callback predicates.
 *  - Layer 4: Recursive path parsing and component resolution.
 *  - Layer 5: Atomic directory slot search and LFN/SFN record injection.
 *  - Layer 6: POSIX-style CRUD APIs (load, create, write, delete, mkdir, rmdir, ls).
 */

#include "fat16.h"

/* ============================================================================
 *  CONSTANTS & FILESYSTEM DEFINITIONS
 * ============================================================================ */

#define ATA_SECTOR_SIZE 512
#define DIR_ENTRY_SIZE  32

/**
 * @struct fat16_cache_t
 * @brief In-memory cache for parsed Volume Boot Record (VBR) geometry.
 *
 * Stores precomputed base LBAs and geometry multipliers to avoid repeated
 * disk I/O and arithmetic on sector 0.
 */
typedef struct {
    uint32_t fat_start_sector;       /* LBA where primary FAT (FAT1) begins */
    uint32_t root_dir_sector;        /* LBA where the root directory table begins */
    uint32_t data_start_sector;      /* LBA where data cluster 2 starts */
    uint32_t root_dir_size_sectors;  /* Number of sectors reserved for the root directory */
    uint16_t sectors_per_fat;        /* Sector count for a single FAT instance */
    uint8_t  fat_count;              /* Total count of FAT copies (usually 2) */
    uint8_t  sectors_per_cluster;    /* Sectors per data allocation unit */
    uint32_t total_sectors;          /* Total volume sectors (16-bit or 32-bit container) */
    uint16_t total_clusters;         /* Total number of data clusters (>= 2) */
    int      initialized;            /* Flag indicating whether cache has been loaded */
} fat16_cache_t;

/* Global filesystem geometry cache */
static fat16_cache_t fs_cache = {0};

/* ============================================================================
 *  INITIALIZATION & SETUP
 * ============================================================================ */

/**
 * @brief Initializes the driver by parsing the BIOS Parameter Block (BPB) at LBA 0.
 *
 * Computes boundary sectors for FAT tables, root directory, and data area.
 * Bails out safely if sector 0 cannot be read from the disk controller.
 */
void fat16_init(void) {
    uint8_t buffer[512];
    if (ata_read_sector(0, buffer) != 0) {
        return;
    }
    bpb_t* bpb = (bpb_t*)buffer;

    /* Extract core geometric parameters */
    fs_cache.fat_start_sector      = bpb->reserved_sectors;
    fs_cache.sectors_per_fat       = bpb->sectors_per_fat;
    fs_cache.fat_count             = bpb->fat_count;
    fs_cache.sectors_per_cluster   = bpb->sectors_per_cluster;
    
    /* Root directory immediately follows all mirrored FAT tables */
    fs_cache.root_dir_sector       = fs_cache.fat_start_sector + 
                                     (fs_cache.fat_count * fs_cache.sectors_per_fat);
    
    /* Root directory size must be calculated before computing the data region base */
    fs_cache.root_dir_size_sectors = (bpb->root_dir_entries * 32) / 512;
    fs_cache.data_start_sector     = fs_cache.root_dir_sector + fs_cache.root_dir_size_sectors;

    /* Extract total sector count (handle 16-bit vs 32-bit field) */
    uint32_t tot_sec = bpb->total_sectors_16;
    if (tot_sec == 0) {
        tot_sec = *(uint32_t*)&buffer[32];
    }
    fs_cache.total_sectors  = tot_sec;
    fs_cache.total_clusters = (fs_cache.total_sectors - fs_cache.data_start_sector) / 
                              fs_cache.sectors_per_cluster;

    fs_cache.initialized = 1;
}

/**
 * @brief Guard helper ensuring BPB parameters are loaded before any disk action.
 */
static void ensure_init(void) {
    if (!fs_cache.initialized) {
        fat16_init();
    }
}

/* ============================================================================
 *  LAYER 1: FAT TABLE & CLUSTER ALLOCATION
 * ============================================================================ */

/**
 * @brief Reads a 16-bit FAT entry corresponding to the specified cluster.
 *
 * @param cluster Cluster index to look up (valid: 2..total_clusters+1).
 * @return Next cluster value, EOF marker (>= 0xFFF8), or 0xFFFF on read error.
 */
static uint16_t fat16_read_fat_entry(uint16_t cluster) {
    ensure_init();
    
    uint32_t fat_sector = fs_cache.fat_start_sector + ((cluster * 2) / 512);
    uint32_t fat_offset = (cluster * 2) % 512;

    uint8_t buffer[512];
    if (ata_read_sector(fat_sector, buffer) != 0) {
        return 0xFFFF;
    }
    
    return *(uint16_t*)&buffer[fat_offset];
}

/**
 * @brief Writes a 16-bit value to a FAT entry and mirrors it across all FAT copies.
 *
 * @param cluster Cluster index to update.
 * @param value New pointer/state (0x0000 = free, 0xFFFF = EOF, or next cluster).
 * @return EOK on success, -EIO on disk controller failure.
 */
static int fat16_write_fat_entry(uint16_t cluster, uint16_t value) {
    ensure_init();
    
    uint32_t sector_offset = (cluster * 2) / 512;
    uint32_t byte_offset   = (cluster * 2) % 512;

    uint8_t buffer[512];
    uint32_t primary_sec = fs_cache.fat_start_sector + sector_offset;

    /* Read primary FAT sector to preserve adjacent cluster entries */
    if (ata_read_sector(primary_sec, buffer) != 0) {
        return -EIO;
    }

    *(uint16_t*)&buffer[byte_offset] = value;

    /* Write updated sector to every mirrored FAT table */
    for (uint32_t i = 0; i < fs_cache.fat_count; i++) {
        uint32_t target_sec = fs_cache.fat_start_sector + 
                              (i * fs_cache.sectors_per_fat) + sector_offset;
        if (ata_write_sector(target_sec, buffer) != 0) {
            return -EIO;
        }
    }

    return EOK;
}

/**
 * @brief Scans the FAT table to allocate the first available cluster.
 *
 * Marks the newly allocated cluster with an EOF terminator (0xFFFF) immediately.
 *
 * @return Allocated cluster number (>= 2), or 0 if the volume is full or I/O fails.
 */
static uint16_t fat16_alloc_cluster(void) {
    ensure_init();
    uint8_t buffer[512];

    for (uint32_t s = 0; s < fs_cache.sectors_per_fat; s++) {
        if (ata_read_sector(fs_cache.fat_start_sector + s, buffer) != 0) {
            return 0;
        }
        
        uint16_t* table = (uint16_t*)buffer;

        for (int i = 0; i < 256; i++) {
            uint16_t cluster_idx = (s * 256) + i;
            if (cluster_idx < 2) continue; /* Clusters 0 and 1 are reserved */
            if (cluster_idx >= fs_cache.total_clusters + 2) return 0;

            if (table[i] == 0x0000) {
                fat16_write_fat_entry(cluster_idx, 0xFFFF);
                return cluster_idx;
            }
        }
    }
    return 0;
}

/**
 * @brief Traverses a linked cluster chain and frees every link by resetting it to 0x0000.
 *
 * @param start_cluster First cluster of the chain to deallocate.
 */
static void fat16_free_chain(uint16_t start_cluster) {
    if (start_cluster < 2 || start_cluster >= 0xFFF8) return;

    uint16_t curr = start_cluster;
    uint32_t safety_counter = fs_cache.total_clusters; /* Guard against cyclic loops */

    while (curr >= 2 && curr < 0xFFF0 && safety_counter--) {
        uint16_t next = fat16_read_fat_entry(curr);
        fat16_write_fat_entry(curr, 0x0000);
        curr = next;
    }
}

/* ============================================================================
 *  LAYER 2: SFN / LFN FORMATTING & CHECKSUMS
 * ============================================================================ */

/**
 * @brief Compares an on-disk 8.3 space-padded SFN against a null-terminated target name.
 *
 * Normalizes the 8.3 entry to lowercase and appends an extension dot if applicable.
 *
 * @param entry Pointer to 32-byte directory entry record.
 * @param target Lowercase or mixed-case filename to match against.
 * @return 1 on exact case-insensitive match, 0 otherwise.
 */
static int match_sfn(const dir_entry_t* entry, const char* target) {
    char sfn[13];
    int idx = 0;

    /* Copy base filename (up to 8 chars) */
    for (int j = 0; j < 8; j++) {
        if (entry->filename[j] != ' ') {
            char c = entry->filename[j];
            if (c >= 'A' && c <= 'Z') c += 32;
            sfn[idx++] = c;
        }
    }

    /* Append extension if present */
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

    /* Case-insensitive string comparison */
    int i = 0;
    while (sfn[i] != '\0' && target[i] != '\0') {
        char t = target[i];
        if (t >= 'A' && t <= 'Z') t += 32;
        if (sfn[i] != t) return 0;
        i++;
    }

    return (sfn[i] == '\0' && target[i] == '\0') ? 1 : 0;
}

/**
 * @brief Calculates the standard 8-bit FAT checksum over an 11-byte short filename.
 *
 * Required by LFN specifications to associate LFN records with their trailing SFN record.
 *
 * @param short_name Raw 11-byte 8.3 directory buffer.
 * @return 8-bit rotated checksum.
 */
static uint8_t lfn_checksum(const uint8_t* short_name) {
    uint8_t sum = 0;
    for (int i = 11; i != 0; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *short_name++;
    }
    return sum;
}

/**
 * @brief Constructs an 11-byte 8.3 short filename with numeric tail from a long filename.
 *
 * Example: "verylongname.txt" -> "VERYLO~1.TXT" (attempt 1) or "VERYLO~2.TXT" (attempt 2).
 *
 * @param lfn Source long filename string.
 * @param sfn_out Destination 11-byte buffer (space-padded, no null terminator).
 * @param attempt Numeric tail value (~1, ~2, ..., ~999).
 */
static void generate_short_name(const char* lfn, uint8_t* sfn_out, int attempt) {
    memset(sfn_out, ' ', 11);

    char base_name[8];
    int base_len = 0;
    int i = 0;

    /* Extract base name up to 8 characters or first period */
    while (lfn[i] != '\0' && base_len < 8 && lfn[i] != '.') {
        char c = lfn[i];
        if (c != ' ') {
            if (c >= 'a' && c <= 'z') c -= 32; /* Uppercase */
            base_name[base_len++] = c;
        }
        i++;
    }

    /* Calculate characters to preserve before appending the numeric tail (~N) */
    int suffix_len = (attempt < 10) ? 2 : ((attempt < 100) ? 3 : 4);
    int keep_len = (base_len + suffix_len > 8) ? (8 - suffix_len) : base_len;

    for (int j = 0; j < keep_len; j++) {
        sfn_out[j] = base_name[j];
    }

    /* Append '~' and numeric sequence */
    sfn_out[keep_len] = '~';
    if (attempt < 10) {
        sfn_out[keep_len + 1] = attempt + '0';
    } else if (attempt < 100) {
        sfn_out[keep_len + 1] = (attempt / 10) + '0';
        sfn_out[keep_len + 2] = (attempt % 10) + '0';
    } else {
        sfn_out[keep_len + 1] = (attempt / 100) + '0';
        sfn_out[keep_len + 2] = ((attempt / 10) % 10) + '0';
        sfn_out[keep_len + 3] = (attempt % 10) + '0';
    }

    /* Find last period to identify the extension */
    int last_dot = -1;
    int len = 0;
    while (lfn[len] != '\0') {
        if (lfn[len] == '.') last_dot = len;
        len++;
    }

    /* Copy up to 3 uppercase extension characters starting at offset 8 */
    if (last_dot != -1) {
        int ext_idx = 8;
        i = last_dot + 1;
        while (lfn[i] != '\0' && ext_idx < 11) {
            char c = lfn[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            sfn_out[ext_idx++] = c;
            i++;
        }
    }
}

/* ============================================================================
 *  LAYER 3: UNIFIED DIRECTORY ITERATION ENGINE
 * ============================================================================ */

/**
 * @brief Callback signature for directory entry visitor functions.
 *
 * @param sector Sector LBA currently being read.
 * @param slot_idx Index of entry inside the sector (0..15).
 * @param entry In-memory copy/pointer to the 32-byte directory entry.
 * @param context Caller-supplied state pointer.
 * @return 0 to continue iteration, non-zero to halt and return that value.
 */
typedef int (*dir_entry_callback_t)(uint32_t sector, int slot_idx, dir_entry_t* entry, void* context);

/**
 * @brief Iterates over every 32-byte directory entry in root or any subdirectory.
 *
 * Transparently traverses fixed root directory sectors (when dir_cluster == 0)
 * or dynamically walks a linked cluster chain for subdirectories.
 *
 * @param dir_cluster Starting cluster of the directory (0 for root).
 * @param callback Function invoked per entry slot.
 * @param context State passed directly to the callback.
 * @return Result of the terminating callback or EOK if all entries are visited.
 */
static int iterate_directory(uint16_t dir_cluster, dir_entry_callback_t callback, void* context) {
    uint8_t buffer[512];

    if (dir_cluster == 0) {
        /* Root directory is fixed in the reserved root area */
        for (uint32_t s = 0; s < fs_cache.root_dir_size_sectors; s++) {
            uint32_t sec = fs_cache.root_dir_sector + s;
            if (ata_read_sector(sec, buffer) != 0) return -EIO;

            dir_entry_t* dir = (dir_entry_t*)buffer;
            for (int i = 0; i < 16; i++) {
                int action = callback(sec, i, &dir[i], context);
                if (action < 0) return action; // Propaget error code
                if (action > 0) return EOK; // Normal early exit
            }
        }
    } else {
        /* Subdirectories are stored across data cluster chains */
        uint16_t curr = dir_cluster;
        while (curr >= 2 && curr < 0xFFF8) {
            uint32_t base_sec = fs_cache.data_start_sector + 
                                ((curr - 2) * fs_cache.sectors_per_cluster);

            for (uint8_t s = 0; s < fs_cache.sectors_per_cluster; s++) {
                uint32_t sec = base_sec + s;
                if (ata_read_sector(sec, buffer) != 0) return -EIO;

                dir_entry_t* dir = (dir_entry_t*)buffer;
                for (int i = 0; i < 16; i++) {
                    int action = callback(sec, i, &dir[i], context);
                    if (action < 0) return action; // Propagate error code
                    if (action > 0) return EOK; // Normal early exit
                }
            }
            curr = fat16_read_fat_entry(curr);
        }
    }

    return EOK;
}

/**
 * @struct find_file_ctx_t
 * @brief Context payload used by find_file_callback during searches.
 */
typedef struct {
    const char* target_name;     /* Filename being searched */
    dir_entry_t out_entry;       /* Matched directory entry record */
    uint32_t    out_sector;      /* Disk sector where match lives */
    uint32_t    out_idx;         /* Slot index inside that sector */
    char        lfn_buffer[256]; /* Buffer assembling multi-part LFN names */
    int         found;           /* Flag set to 1 on positive match */
} find_file_ctx_t;

/**
 * @brief Visitor callback that reassembles LFN entries and compares against target.
 */
static int find_file_callback(uint32_t sector, int slot_idx, dir_entry_t* entry, void* context) {
    find_file_ctx_t* ctx = (find_file_ctx_t*)context;
    uint8_t first_byte = entry->filename[0];

    /* 0x00 marks end of active directory entries */
    if (first_byte == 0x00) {
        return 1; /* Terminate search */
    }
    
    /* 0xE5 marks a deleted tombstone entry; clear orphan LFN fragments */
    if (first_byte == 0xE5) {
        ctx->lfn_buffer[0] = '\0';
        return 0;
    }

    /* Attribute 0x0F identifies an LFN entry fragment */
    if (entry->attributes == FAT_ATTR_LFN) {
        struct lfn_entry* lfn = (struct lfn_entry*)entry;
        int index = (lfn->sequence_number & 0x1F) - 1;
        if (index >= 0 && index < 20) {
            int offset = index * 13;
            for (int j = 0; j < 5; j++) ctx->lfn_buffer[offset++] = (lfn->name1[j] == 0xFFFF) ? '\0' : (char)(lfn->name1[j] & 0xFF);
            for (int j = 0; j < 6; j++) ctx->lfn_buffer[offset++] = (lfn->name2[j] == 0xFFFF) ? '\0' : (char)(lfn->name2[j] & 0xFF);
            for (int j = 0; j < 2; j++) ctx->lfn_buffer[offset++] = (lfn->name3[j] == 0xFFFF) ? '\0' : (char)(lfn->name3[j] & 0xFF);
        }
        return 0;
    }

    /* Non-volume label real entry */
    if (entry->attributes != FAT_ATTR_VOLUME_ID) {
        int matched = 0;
        if (ctx->lfn_buffer[0] != '\0' && strcmp(ctx->lfn_buffer, ctx->target_name) == 0) {
            matched = 1;
        } else if (match_sfn(entry, ctx->target_name)) {
            matched = 1;
        }

        if (matched) {
            ctx->out_entry  = *entry;
            ctx->out_sector = sector;
            ctx->out_idx    = slot_idx;
            ctx->found      = 1;
            return 1; /* Match found, stop traversal */
        }
        ctx->lfn_buffer[0] = '\0';
    }

    return 0;
}

/**
 * @brief Searches a specific directory for a named file or folder.
 *
 * @param dir_cluster Directory cluster to search (0 = root).
 * @param name Filename or folder name to locate.
 * @param out_entry [out] Receives copy of directory entry.
 * @param out_sec [out] Receives disk sector where entry resides.
 * @param out_idx [out] Receives entry slot index (0..15).
 * @return EOK on success, -ENOENT if not found.
 */
static int fat16_find_in_dir(uint16_t dir_cluster, const char* name, dir_entry_t* out_entry, uint32_t* out_sec, uint32_t* out_idx) {
    find_file_ctx_t ctx;
    memset((uint8_t*)&ctx, 0, sizeof(ctx));
    ctx.target_name = name;

    iterate_directory(dir_cluster, find_file_callback, &ctx);
    if (ctx.found) {
        if (out_entry) *out_entry = ctx.out_entry;
        if (out_sec)   *out_sec   = ctx.out_sector;
        if (out_idx)   *out_idx   = ctx.out_idx;
        return EOK;
    }

    return -ENOENT;
}

/* ============================================================================
 *  LAYER 4: PATH RESOLUTION ENGINE
 * ============================================================================ */

/**
 * @brief Recursively traverses a full path, resolving intermediate directory clusters.
 *
 * Example: "/sub/folder/file.txt"
 *  - Resolves "sub" in cluster 0.
 *  - Resolves "folder" in "sub"'s starting cluster.
 *  - Returns "folder"'s cluster as out_parent_cluster and "file.txt" as out_final_name.
 *
 * @param path Canonical absolute or relative path string.
 * @param out_parent_cluster [out] Directory cluster hosting the final component.
 * @param out_final_name [out] Buffer (>= 256 bytes) receiving the final filename.
 * @param out_entry [out] Optional pointer receiving metadata if target exists.
 * @return EOK on success, -ENOTDIR, -ENOENT, or -EINVAL on errors.
 */
static int fat16_resolve_path(const char* path, uint16_t* out_parent_cluster, char* out_final_name, dir_entry_t* out_entry) {
    ensure_init();

    if (!path || path[0] == '\0') return -EINVAL;

    uint16_t current_dir = 0; /* 0 represents the Root Directory */
    int i = 0;
    if (path[0] == '/') i++;

    char component[256];

    while (path[i] != '\0') {
        int c_idx = 0;
        while (path[i] != '/' && path[i] != '\0' && c_idx < 255) {
            component[c_idx++] = path[i++];
        }
        component[c_idx] = '\0';

        /* Ignore redundant consecutive slashes */
        while (path[i] == '/') i++;

        /* Handle self-references */
        if (strcmp(component, ".") == 0) {
            continue;
        }

        /* If there are more components remaining, this segment must be a directory */
        if (path[i] != '\0') {
            dir_entry_t entry;
            int res = fat16_find_in_dir(current_dir, component, &entry, NULL, NULL);
            if (res != EOK) return res;

            if (!(entry.attributes & FAT_ATTR_DIRECTORY)) {
                return -ENOTDIR;
            }

            current_dir = entry.starting_cluster;
        } else {
            /* Final target component reached */
            if (out_parent_cluster) *out_parent_cluster = current_dir;
            if (out_final_name)     strcpy(out_final_name, component);
            if (out_entry) {
                fat16_find_in_dir(current_dir, component, out_entry, NULL, NULL);
            }
            return EOK;
        }
    }

    return -EINVAL;
}

/* ============================================================================
 *  LAYER 5: ENTRY ALLOCATION (LFN + SFN INJECTION)
 * ============================================================================ */

/**
 * @struct slot_search_ctx_t
 * @brief Context used to identify contiguous unallocated directory slots.
 */
typedef struct {
    int slots_needed;       /* Total contiguous slots required (LFN entries + 1 SFN) */
    int consecutive_found;  /* Tally of consecutive available slots seen */
    int start_sector;       /* Sector LBA of the first available slot */
    int start_slot;         /* Index (0..15) of the first available slot */
    int done;               /* Set to 1 once a sufficient slot window is found */
} slot_search_ctx_t;

/**
 * @brief Visitor callback locating contiguous deleted (0xE5) or unallocated (0x00) slots.
 */
static int slot_search_callback(uint32_t sector, int slot_idx, dir_entry_t* entry, void* context) {
    slot_search_ctx_t* ctx = (slot_search_ctx_t*)context;
    uint8_t first_byte = entry->filename[0];

    if (first_byte == 0x00 || first_byte == 0xE5) {
        if (ctx->consecutive_found == 0) {
            ctx->start_sector = sector;
            ctx->start_slot   = slot_idx;
        }
        ctx->consecutive_found++;
        if (ctx->consecutive_found == ctx->slots_needed) {
            ctx->done = 1;
            return 1; /* Required slot block found */
        }
    } else {
        ctx->consecutive_found = 0;
    }

    return 0;
}

/**
 * @brief Allocates slots and writes LFN fragments followed by the main SFN entry.
 *
 * Handles numeric tail collision avoidance, computes LFN checksums, and spans
 * directory records across consecutive sectors if a sector boundary is crossed.
 *
 * @param parent_cluster Directory cluster to receive the entry (0 = root).
 * @param name Full filename.
 * @param attributes File/directory attributes (e.g. FAT_ATTR_ARCHIVE, FAT_ATTR_DIRECTORY).
 * @param cluster Starting cluster pointer for the entry.
 * @param size Initial file size in bytes.
 * @return EOK on success, -ENOSPC if directory is full, or -EIO on disk write failure.
 */
static int fat16_insert_entry(uint16_t parent_cluster, const char* name, uint8_t attributes, uint16_t cluster, uint32_t size) {
    int name_len = strlen(name);
    int lfn_count = (name_len + 12) / 13;
    int total_slots = lfn_count + 1;

    /* Generate collision-free SFN numeric tail (~1, ~2, ...) */
    uint8_t sfn[11];
    int attempt = 1;
    while (1) {
        generate_short_name(name, sfn, attempt);
        dir_entry_t existing;
        char temp_target[13];
        int t_idx = 0;
        for (int k = 0; k < 8; k++) if (sfn[k] != ' ') temp_target[t_idx++] = sfn[k];
        if (sfn[8] != ' ') {
            temp_target[t_idx++] = '.';
            for (int k = 8; k < 11; k++) if (sfn[k] != ' ') temp_target[t_idx++] = sfn[k];
        }
        temp_target[t_idx] = '\0';

        if (fat16_find_in_dir(parent_cluster, temp_target, &existing, NULL, NULL) != EOK) {
            break; /* Available SFN found */
        }
        attempt++;
    }

    uint8_t chksum = lfn_checksum(sfn);

    /* Search for consecutive empty directory slots */
    slot_search_ctx_t ctx;
    memset((uint8_t*)&ctx, 0, sizeof(ctx));
    ctx.slots_needed = total_slots;

    iterate_directory(parent_cluster, slot_search_callback, &ctx);
    if (!ctx.done) {
        return -ENOSPC;
    }

    /* Stream LFN records and final SFN record to disk */
    uint8_t sec_buf[512];
    int current_sec = ctx.start_sector;
    if (ata_read_sector(current_sec, sec_buf) != 0) return -EIO;

    int slot = ctx.start_slot;

    for (int i = 0; i < total_slots; i++) {
        if (i < lfn_count) {
            /* LFN records are placed in reverse sequence order on disk */
            int lfn_idx = lfn_count - i;
            int char_offset = (lfn_idx - 1) * 13;

            struct lfn_entry* lfn = (struct lfn_entry*)(sec_buf + (slot * 32));
            memset((uint8_t*)lfn, 0, 32);

            lfn->sequence_number = lfn_idx;
            if (i == 0) lfn->sequence_number |= 0x40; /* 0x40 marks last LFN physical record */
            lfn->attribute     = FAT_ATTR_LFN;
            lfn->type          = 0;
            lfn->checksum      = chksum;
            lfn->first_cluster = 0;

            /* Expand ASCII characters into 16-bit UCS-2 units */
            uint16_t chars[13];
            for (int c = 0; c < 13; c++) {
                int pos = char_offset + c;
                if (pos < name_len) chars[c] = (uint16_t)name[pos];
                else if (pos == name_len) chars[c] = 0x0000;
                else chars[c] = 0xFFFF;
            }

            for (int c = 0; c < 5; c++) lfn->name1[c] = chars[c];
            for (int c = 0; c < 6; c++) lfn->name2[c] = chars[5 + c];
            for (int c = 0; c < 2; c++) lfn->name3[c] = chars[11 + c];
        } else {
            /* Trailing real 8.3 directory entry */
            dir_entry_t* dir = (dir_entry_t*)(sec_buf + (slot * 32));
            memset((uint8_t*)dir, 0, 32);
            memcpy(dir->filename, sfn, 11);
            dir->attributes       = attributes;
            dir->starting_cluster = cluster;
            dir->file_size        = size;
        }

        slot++;
        /* Advance to next sector if crossing 512-byte sector boundary */
        if (slot >= 16) {
            if (ata_write_sector(current_sec, sec_buf) != 0) return -EIO;
            current_sec++;
            if (ata_read_sector(current_sec, sec_buf) != 0) return -EIO;
            slot = 0;
        }
    }

    if (ata_write_sector(current_sec, sec_buf) != 0) {
        return -EIO;
    }

    return EOK;
}

/* ============================================================================
 *  LAYER 6: HIGH-LEVEL FILE & DIRECTORY CRUD
 * ============================================================================ */

/**
 * @brief Reads an entire file into a dynamically allocated heap buffer.
 *
 * @param path Canonical file path.
 * @param out_file_size [out] Optional pointer receiving byte size.
 * @return Heap buffer pointer containing content (must be freed with kfree), or NULL.
 */
uint8_t* fat16_load_file(const char* path, uint32_t* out_file_size) {
    ensure_init();

    uint16_t parent_cluster;
    char filename[256];
    dir_entry_t entry;

    int res = fat16_resolve_path(path, &parent_cluster, filename, &entry);
    if (res != EOK) return NULL;

    if (fat16_find_in_dir(parent_cluster, filename, &entry, NULL, NULL) != EOK) {
        return NULL;
    }

    if (entry.attributes & FAT_ATTR_DIRECTORY) {
        return NULL;
    }

    if (entry.file_size == 0) {
        if (out_file_size) *out_file_size = 0;
        uint8_t* empty_buffer = (uint8_t*)kmalloc(1);
        if (empty_buffer) empty_buffer[0] = '\0';
        return empty_buffer;
    }

    uint8_t* ram_buffer = (uint8_t*)kmalloc(entry.file_size);
    if (!ram_buffer) return NULL;

    uint16_t curr_cluster = entry.starting_cluster;
    uint32_t bytes_left   = entry.file_size;
    uint32_t write_offset = 0;
    uint8_t sector_buf[512];

    /* Walk the cluster chain and read data blocks */
    while (curr_cluster >= 2 && curr_cluster < 0xFFF8 && bytes_left > 0) {
        uint32_t cluster_lba = fs_cache.data_start_sector + 
            ((curr_cluster - 2) * fs_cache.sectors_per_cluster);

        for (uint8_t s = 0; s < fs_cache.sectors_per_cluster && bytes_left > 0; s++) {
            if (ata_read_sector(cluster_lba + s, sector_buf) != 0) {
                kfree(ram_buffer);
                return NULL;
            }

            uint32_t chunk = (bytes_left < 512) ? bytes_left : 512;
            memcpy(ram_buffer + write_offset, sector_buf, chunk);

            write_offset += chunk;
            bytes_left   -= chunk;
        }

        curr_cluster = fat16_read_fat_entry(curr_cluster);
    }

    if (out_file_size) *out_file_size = entry.file_size;
    return ram_buffer;
}

/**
 * @brief Creates an empty 0-byte file record in the appropriate directory.
 *
 * @param path Destination path for the new file.
 * @return EOK on success, -EEXIST if file exists, or error code.
 */
int fat16_create_file(const char* path) {
    ensure_init();

    uint16_t parent_cluster;
    char filename[256];
    dir_entry_t entry;

    int res = fat16_resolve_path(path, &parent_cluster, filename, &entry);
    if (res != EOK) return res;

    if (fat16_find_in_dir(parent_cluster, filename, &entry, NULL, NULL) == EOK) {
        return -EEXIST;
    }

    return fat16_insert_entry(parent_cluster, filename, FAT_ATTR_ARCHIVE, 0, 0);
}

/**
 * @brief Overwrites or appends data to an existing file, extending cluster chains as needed.
 *
 * @param path Destination file path.
 * @param data Buffer containing payload to write.
 * @param len Byte count of payload.
 * @param append 1 to append to EOF, 0 to truncate and overwrite from beginning.
 * @return EOK on success, -ENOSPC if disk is full, or -EIO on disk error.
 */
int fat16_write_file(const char* path, const char* data, uint32_t len, int append) {
    ensure_init();

    uint16_t parent_cluster;
    char filename[256];
    dir_entry_t entry;
    uint32_t target_sec = 0;
    uint32_t target_idx = 0;

    int res = fat16_resolve_path(path, &parent_cluster, filename, &entry);
    if (res != EOK) return res;

    res = fat16_find_in_dir(parent_cluster, filename, &entry, &target_sec, &target_idx);

    // If the file does not exist, create it first
    if (res == -ENOENT) {
        /* File does not exist; create it first */
        res = fat16_insert_entry(parent_cluster, filename, FAT_ATTR_ARCHIVE, 0, 0);
        if (res != EOK) return res;

        /* Re-resolve to get the new entry */
        res = fat16_find_in_dir(parent_cluster, filename, &entry, &target_sec, &target_idx);
        if (res != EOK) return res;
    } else if (res != EOK) return res;

    if (entry.attributes & FAT_ATTR_DIRECTORY) return -EISDIR;

    /* Truncation to 0 bytes: release clusters and update directory */
    if (!append && len == 0) {
        if (entry.starting_cluster >= 2) {
            fat16_free_chain(entry.starting_cluster);
        }
        entry.starting_cluster = 0;
        entry.file_size = 0;
        goto update_dir;
    }

    uint16_t first_cluster = entry.starting_cluster;
    uint16_t curr_cluster  = 0;
    uint16_t prev_cluster  = 0;

    uint32_t bytes_written = 0;
    uint32_t cluster_bytes = fs_cache.sectors_per_cluster * ATA_SECTOR_SIZE;

    if (!append) {
        /* Truncate prior contents */
        if (entry.starting_cluster >= 2) {
            fat16_free_chain(entry.starting_cluster);
        }
        first_cluster = 0;
    } else {
        /* Traverse to terminal cluster of existing chain */
        if (entry.starting_cluster >= 2) {
            curr_cluster = entry.starting_cluster;
            while (curr_cluster >= 2 && curr_cluster < 0xFFF8) {
                prev_cluster = curr_cluster;
                uint16_t next = fat16_read_fat_entry(curr_cluster);
                if (next >= 0xFFF8) break;
                curr_cluster = next;
            }
        }
    }

    /* Core write loop: allocates clusters and streams sectors */
    while (bytes_written < len) {
        uint32_t in_cluster_offset = 0;
        if (append && entry.file_size > 0 && bytes_written == 0) {
            in_cluster_offset = entry.file_size % cluster_bytes;
        }

        /* Allocate new cluster if crossing a boundary or start of file */
        if (in_cluster_offset == 0 || curr_cluster < 2) {
            uint16_t new_c = fat16_alloc_cluster();
            if (new_c == 0) return -ENOSPC;

            if (first_cluster == 0) first_cluster = new_c;
            if (prev_cluster >= 2)  fat16_write_fat_entry(prev_cluster, new_c);

            curr_cluster = new_c;
            prev_cluster = new_c;
        }

        uint32_t cluster_lba = fs_cache.data_start_sector +
            ((curr_cluster - 2) * fs_cache.sectors_per_cluster);

        uint32_t start_sec  = in_cluster_offset / ATA_SECTOR_SIZE;
        uint32_t sec_offset = in_cluster_offset % ATA_SECTOR_SIZE;

        for (uint8_t s = start_sec; s < fs_cache.sectors_per_cluster && bytes_written < len; s++) {
            uint8_t sector_buf[512];

            /* Preserve existing partial sector data during unaligned append/tail writes */
            if (sec_offset > 0 || (len - bytes_written) < ATA_SECTOR_SIZE) {
                if (ata_read_sector(cluster_lba + s, sector_buf) != 0) return -EIO;
            } else {
                memset(sector_buf, 0, 512);
            }

            uint32_t chunk = ATA_SECTOR_SIZE - sec_offset;
            if (chunk > (len - bytes_written)) chunk = len - bytes_written;

            memcpy(sector_buf + sec_offset, (const uint8_t*)data + bytes_written, chunk);

            if (ata_write_sector(cluster_lba + s, sector_buf) != 0) return -EIO;

            bytes_written += chunk;
            sec_offset = 0;
        }
    }

    if (append) {
        entry.file_size += len;
        entry.starting_cluster = first_cluster;
    } else {
        entry.file_size = len;
        entry.starting_cluster = first_cluster;
    }

update_dir:
    /* Commit updated file size and starting cluster to directory entry */
    {
        uint8_t dir_buf[512];
        if (ata_read_sector(target_sec, dir_buf) != 0) return -EIO;

        dir_entry_t* dir = (dir_entry_t*)dir_buf;
        dir[target_idx] = entry;

        if (ata_write_sector(target_sec, dir_buf) != 0) return -EIO;
    }

    return EOK;
}

/**
 * @brief Deletes a file by releasing its cluster chain and placing tombstone 0xE5.
 *
 * @param path File to delete.
 * @return EOK on success, -EISDIR if target is a folder, or error code.
 */
int fat16_delete_file(const char* path) {
    ensure_init();

    uint16_t parent_cluster;
    char filename[256];
    dir_entry_t entry;
    uint32_t target_sec = 0;
    uint32_t target_idx = 0;

    int res = fat16_resolve_path(path, &parent_cluster, filename, &entry);
    if (res != EOK) return res;

    res = fat16_find_in_dir(parent_cluster, filename, &entry, &target_sec, &target_idx);
    if (res != EOK) return res;

    if (entry.attributes & FAT_ATTR_DIRECTORY) return -EISDIR;

    /* Free linked data clusters */
    if (entry.starting_cluster >= 2) {
        fat16_free_chain(entry.starting_cluster);
    }

    /* Mark directory entry tombstone */
    uint8_t buffer[512];
    if (ata_read_sector(target_sec, buffer) != 0) return -EIO;

    dir_entry_t* dir = (dir_entry_t*)buffer;
    dir[target_idx].filename[0] = 0xE5;

    if (ata_write_sector(target_sec, buffer) != 0) return -EIO;

    return EOK;
}

/**
 * @brief Creates a directory, allocates a cluster, and installs "." and ".." links.
 *
 * @param path Destination folder path.
 * @return EOK on success, -EEXIST if name taken, or error code.
 */
int fat16_make_directory(const char* path) {
    ensure_init();

    uint16_t parent_cluster;
    char dirname[256];
    dir_entry_t entry;

    int res = fat16_resolve_path(path, &parent_cluster, dirname, &entry);
    if (res != EOK) return res;

    if (fat16_find_in_dir(parent_cluster, dirname, &entry, NULL, NULL) == EOK) {
        return -EEXIST;
    }

    /* Allocate cluster for directory table entries */
    uint16_t new_dir_cluster = fat16_alloc_cluster();
    if (new_dir_cluster == 0) return -ENOSPC;

    /* Build "." and ".." directory records */
    uint8_t dir_buf[512];
    memset(dir_buf, 0, 512);
    dir_entry_t* entries = (dir_entry_t*)dir_buf;

    /* "." points to self */
    memset(entries[0].filename, ' ', 11);
    entries[0].filename[0] = '.';
    entries[0].attributes = FAT_ATTR_DIRECTORY;
    entries[0].starting_cluster = new_dir_cluster;

    /* ".." points to parent directory (or 0 for root) */
    memset(entries[1].filename, ' ', 11);
    entries[1].filename[0] = '.';
    entries[1].filename[1] = '.';
    entries[1].attributes = FAT_ATTR_DIRECTORY;
    entries[1].starting_cluster = parent_cluster;

    uint32_t new_dir_lba = fs_cache.data_start_sector + 
                           ((new_dir_cluster - 2) * fs_cache.sectors_per_cluster);

    /* Flush "." and ".." in sector 0 and zero out remaining cluster sectors */
    for (uint8_t s = 0; s < fs_cache.sectors_per_cluster; s++) {
        if (s == 0) {
            if (ata_write_sector(new_dir_lba + s, dir_buf) != 0) return -EIO;
            memset(dir_buf, 0, 512);
        } else {
            if (ata_write_sector(new_dir_lba + s, dir_buf) != 0) return -EIO;
        }
    }

    return fat16_insert_entry(parent_cluster, dirname, FAT_ATTR_DIRECTORY, new_dir_cluster, 0);
}

/**
 * @struct empty_check_ctx_t
 * @brief Context counter used to verify directory vacancy before removal.
 */
typedef struct {
    int count;
} empty_check_ctx_t;

/**
 * @brief Visitor callback checking if any active entries exist besides "." and "..".
 */
static int empty_check_callback(uint32_t sector, int slot_idx, dir_entry_t* entry, void* context) {
    (void)sector; (void)slot_idx;
    empty_check_ctx_t* ctx = (empty_check_ctx_t*)context;
    uint8_t first_byte = entry->filename[0];

    if (first_byte == 0x00) return 1; /* Unallocated space reached */
    if (first_byte == 0xE5) return 0; /* Deleted entry */

    if (entry->attributes != FAT_ATTR_VOLUME_ID && entry->attributes != FAT_ATTR_LFN) {
        /* Ignore "." and ".." relative links */
        if (entry->filename[0] == '.') {
            if (entry->filename[1] == ' ' || (entry->filename[1] == '.' && entry->filename[2] == ' ')) {
                return 0;
            }
        }
        ctx->count++;
        return 1; /* Non-empty directory confirmed */
    }

    return 0;
}

/**
 * @brief Removes an empty directory record and releases its cluster chain.
 *
 * @param path Path to directory.
 * @return EOK on success, -ENOTEMPTY if files exist within, or -ENOTDIR.
 */
int fat16_remove_directory(const char* path) {
    ensure_init();

    uint16_t parent_cluster;
    char dirname[256];
    dir_entry_t entry;
    uint32_t target_sec = 0;
    uint32_t target_idx = 0;

    int res = fat16_resolve_path(path, &parent_cluster, dirname, &entry);
    if (res != EOK) return res;

    res = fat16_find_in_dir(parent_cluster, dirname, &entry, &target_sec, &target_idx);
    if (res != EOK) return res;

    if (!(entry.attributes & FAT_ATTR_DIRECTORY)) {
        return -ENOTDIR;
    }

    /* Confirm directory contains no files or subdirectories */
    empty_check_ctx_t check_ctx = {0};
    iterate_directory(entry.starting_cluster, empty_check_callback, &check_ctx);
    if (check_ctx.count > 0) {
        return -ENOTEMPTY;
    }

    /* Free cluster allocation */
    fat16_free_chain(entry.starting_cluster);

    /* Mark entry as deleted (0xE5) */
    uint8_t buffer[512];
    if (ata_read_sector(target_sec, buffer) != 0) return -EIO;

    dir_entry_t* dir = (dir_entry_t*)buffer;
    dir[target_idx].filename[0] = 0xE5;

    if (ata_write_sector(target_sec, buffer) != 0) return -EIO;

    return EOK;
}

/**
 * @struct list_dir_ctx_t
 * @brief Context maintaining LFN buffer across lines when printing directory trees.
 */
typedef struct {
    char lfn_buf[256];
} list_dir_ctx_t;

/**
 * @brief Visitor callback that prints entries with terminal color accents and file sizes.
 */
static int list_dir_callback(uint32_t sector, int slot_idx, dir_entry_t* entry, void* context) {
    (void)sector; (void)slot_idx;
    list_dir_ctx_t* ctx = (list_dir_ctx_t*)context;
    uint8_t first_byte = entry->filename[0];

    if (first_byte == 0x00) return 1; /* Reached end of active table */
    if (first_byte == 0xE5) {
        ctx->lfn_buf[0] = '\0';
        return 0;
    }

    if (entry->attributes == FAT_ATTR_LFN) {
        struct lfn_entry* lfn = (struct lfn_entry*)entry;
        int index = (lfn->sequence_number & 0x1F) - 1;
        if (index >= 0 && index < 20) {
            int offset = index * 13;
            for (int j = 0; j < 5; j++) ctx->lfn_buf[offset++] = (lfn->name1[j] == 0xFFFF) ? '\0' : (char)(lfn->name1[j] & 0xFF);
            for (int j = 0; j < 6; j++) ctx->lfn_buf[offset++] = (lfn->name2[j] == 0xFFFF) ? '\0' : (char)(lfn->name2[j] & 0xFF);
            for (int j = 0; j < 2; j++) ctx->lfn_buf[offset++] = (lfn->name3[j] == 0xFFFF) ? '\0' : (char)(lfn->name3[j] & 0xFF);
        }
        return 0;
    }

    if (entry->attributes != FAT_ATTR_VOLUME_ID) {
        if (entry->attributes & FAT_ATTR_DIRECTORY) {
            set_print_color(0x0B); /* Cyan for folders */
            puts("[DIR]  ");
        } else {
            set_print_color(0x0F);
            puts("       ");
        }

        /* Print reconstructed LFN if present, else print parsed SFN */
        if (ctx->lfn_buf[0] != '\0') {
            puts(ctx->lfn_buf);
        } else {
            for (int j = 0; j < 8; j++) {
                if (entry->filename[j] != ' ') print_char(entry->filename[j]);
            }
            if (entry->extension[0] != ' ') {
                print_char('.');
                for (int j = 0; j < 3; j++) {
                    if (entry->extension[j] != ' ') print_char(entry->extension[j]);
                }
            }
        }

        if (!(entry->attributes & FAT_ATTR_DIRECTORY)) {
            set_print_color(0x07);
            puts(" (");
            print_int(entry->file_size);
            puts(" B)");
        }

        puts("\n");
        set_print_color(0x0F);
        ctx->lfn_buf[0] = '\0';
    }

    return 0;
}

/**
 * @brief Lists all files and subdirectories within a specified canonical path.
 *
 * @param path Directory path to inspect ("/" or empty string for root).
 * @return EOK on success, or -ENOTDIR / -ENOENT on resolution failure.
 */
int fat16_list_directory(const char* path) {
    ensure_init();

    uint16_t target_cluster = 0;

    if (path && path[0] != '\0' && strcmp(path, "/") != 0) {
        uint16_t parent_cluster;
        char dirname[256];
        dir_entry_t entry;

        int res = fat16_resolve_path(path, &parent_cluster, dirname, &entry);
        if (res != EOK){ 
            set_print_color(0x0F);
            return res; 
        }

        res = fat16_find_in_dir(parent_cluster, dirname, &entry, NULL, NULL);
        if (res != EOK) {
            set_print_color(0x0F);
            return res;
        }

        if (!(entry.attributes & FAT_ATTR_DIRECTORY)) {
            set_print_color(0x0F);
            return -ENOTDIR;
        }

        target_cluster = entry.starting_cluster;
    }

    list_dir_ctx_t ctx;
    memset((uint8_t*)&ctx, 0, sizeof(ctx));

    int status = iterate_directory(target_cluster, list_dir_callback, &ctx);

    set_print_color(0x0F);
    
    return status;
}