#include "fat32.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> //Hugh: I have no idea why compiler was letting you use bool without this... C DOES NOT HAVE A BOOL DATATYPE NATIVELY

/* Little-endian readers */
static uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* Mount FAT32 filesystem */
bool fs_mount(FileSystem *fs, const char *image_path) {
    memset(fs, 0, sizeof(*fs));

    fs->image = fopen(image_path, "r+b");
    if (!fs->image) {
        fprintf(stderr, "Error: cannot open image file '%s'\n", image_path);
        return false;
    }

    strncpy(fs->image_name, image_path, sizeof(fs->image_name)-1);

    unsigned char boot[512];
    if (fread(boot, 1, sizeof(boot), fs->image) != 512) {
        fprintf(stderr, "Error: cannot read boot sector\n");
        fclose(fs->image);
        fs->image = NULL;
        return false;
    }

    Fat32BootSector *bpb = &fs->bpb;

    bpb->bytes_per_sector      = read_le16(&boot[0x0B]);
    bpb->sectors_per_cluster   = boot[0x0D];
    bpb->reserved_sector_count = read_le16(&boot[0x0E]);
    bpb->num_fats              = boot[0x10];

    uint16_t total16 = read_le16(&boot[0x13]);
    uint32_t total32 = read_le32(&boot[0x20]);
    bpb->total_sectors = (total16 != 0) ? total16 : total32;

    bpb->fat_size_sectors = read_le32(&boot[0x24]);
    bpb->root_cluster = boot[0x2C];

     /* Compute commonly used layout values */
    uint32_t bytes_per_sector    = bpb->bytes_per_sector;
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;

    /* Sector index at which the first FAT begins */
    fs->fat_start_sector = bpb->reserved_sector_count;

    /* Sector index at which the data region (cluster 2) begins */
    fs->first_data_sector =
        bpb->reserved_sector_count + (bpb->num_fats * bpb->fat_size_sectors);

    /* Total number of clusters in the data region */
    uint32_t data_sectors =
        bpb->total_sectors - (fs->first_data_sector);
    fs->total_clusters = data_sectors / sectors_per_cluster;

    /* Start with current working directory at the root cluster */
    fs->cwd_cluster = bpb->root_cluster;

    fs->fat_end_sector = fs->fat_start_sector + bpb->fat_size_sectors;

    return true;
}

/* Unmount filesystem */
void fs_unmount(FileSystem *fs) {
    if (fs->image) {
        fclose(fs->image);
        fs->image = NULL;
    }
}

/* info command */
void cmd_info(const FileSystem *fs) {

    const Fat32BootSector *bpb = &fs->bpb;

    uint32_t bytes_per_sector    = bpb->bytes_per_sector;
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;
    uint32_t reserved            = bpb->reserved_sector_count;
    uint32_t num_fats            = bpb->num_fats;
    uint32_t fat_size            = bpb->fat_size_sectors;
    uint32_t total_sectors       = bpb->total_sectors;
    uint32_t root_cluster        = bpb->root_cluster;

    uint32_t data_sectors = total_sectors - (reserved + num_fats * fat_size);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;

    uint32_t entries_per_fat = (fat_size * bytes_per_sector) / 4;

    unsigned long long image_bytes =
        (unsigned long long)total_sectors * (unsigned long long)bytes_per_sector;

    printf("position of root cluster (in cluster #): %u\n", root_cluster);
    printf("bytes per sector: %u\n", bytes_per_sector);
    printf("sectors per cluster: %u\n", sectors_per_cluster);
    printf("total # of clusters in data region: %u\n", total_clusters);
    printf("# of entries in one FAT: %u\n", entries_per_fat);
    printf("size of image (in bytes): %llu\n", image_bytes);

}

#define FAT32_EOC 0x0FFFFFFF

/* Convert a cluster number to a byte offset in the image file */
static long cluster_to_offset(const FileSystem *fs, uint32_t cluster) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t bytes_per_sector    = bpb->bytes_per_sector;
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;

    uint32_t first_data_sector = fs->first_data_sector;
    uint32_t sector =
        first_data_sector + (cluster - 2) * sectors_per_cluster;

    return (long)sector * bytes_per_sector;
}

static void build_short_name(char dest[11], const char *name) {
    memset(dest, ' ', 11);

    size_t len = strlen(name);
    if (len > 11) len = 11;

    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        dest[i] = c;
    }
}

/* Read a FAT32 entry for a given cluster.
 *
 * Each FAT32 entry is 32 bits (4 bytes).
 * FAT offset = cluster * 4
 * FAT sector = fat_start_sector + (fat_offset / bytes_per_sector)
 * entry offset = fat_offset % bytes_per_sector
 *
 * This function returns:
 *   - next cluster in chain
 *   - FAT32_EOC when at end of cluster chain
 */
static uint32_t read_fat_entry(FileSystem *fs, uint32_t cluster) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_sector + (fat_offset / bpb->bytes_per_sector);
    uint32_t ent_offset = fat_offset % bpb->bytes_per_sector;

    long byte_offset = (long)fat_sector * bpb->bytes_per_sector + ent_offset;
    if (fseek(fs->image, byte_offset, SEEK_SET) != 0) {
        return FAT32_EOC;
    }

    unsigned char buf[4];
    if (fread(buf, 1, 4, fs->image) != 4) {
        return FAT32_EOC;
    }

    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

/* Write FAT32 entry for a given cluster */
static void write_fat_entry(FileSystem *fs, uint32_t cluster, uint32_t value) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_sector + (fat_offset / bpb->bytes_per_sector);
    uint32_t ent_offset = fat_offset % bpb->bytes_per_sector;

    long byte_offset = (long)fat_sector * bpb->bytes_per_sector + ent_offset;
    if (fseek(fs->image, byte_offset, SEEK_SET) != 0) {
        return;
    }

    unsigned char buf[4];
    buf[0] = (unsigned char)(value & 0xFF);
    buf[1] = (unsigned char)((value >> 8) & 0xFF);
    buf[2] = (unsigned char)((value >> 16) & 0xFF);
    buf[3] = (unsigned char)((value >> 24) & 0xFF);

    fwrite(buf, 1, 4, fs->image);
}

/* Find a free cluster by scanning the FAT */
static uint32_t allocate_cluster(FileSystem *fs) {
    for (uint32_t c = 2; c < fs->total_clusters + 2; c++) {
        uint32_t val = read_fat_entry(fs, c);
        if (val == 0x00000000) {
            write_fat_entry(fs, c, FAT32_EOC);
            return c;
        }
    }
    return 0; /* No free cluster */
}

/* Initialize a newly allocated directory cluster with "." and ".." entries */
static void init_directory_cluster(FileSystem *fs,
                                   uint32_t new_cluster,
                                   uint32_t parent_cluster) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size =
        bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)calloc(1, cluster_size);
    if (!buf) return;

    /* "." entry (self) */
    unsigned char *dot = buf;
    memset(dot, ' ', 11);
    dot[0] = '.';
    dot[11] = 0x10; 

    /* Cluster number for "." */
    uint32_t cl = new_cluster;
    dot[20] = (unsigned char)((cl >> 24) & 0xFF);
    dot[21] = (unsigned char)((cl >> 16) & 0xFF);
    dot[26] = (unsigned char)(cl & 0xFF);
    dot[27] = (unsigned char)((cl >> 8) & 0xFF);

    /* ".." entry (parent) */
    unsigned char *dotdot = buf + 32;
    memset(dotdot, ' ', 11);
    dotdot[0] = '.';
    dotdot[1] = '.';
    dotdot[11] = 0x10; 

    cl = parent_cluster;
    dotdot[20] = (unsigned char)((cl >> 24) & 0xFF);
    dotdot[21] = (unsigned char)((cl >> 16) & 0xFF);
    dotdot[26] = (unsigned char)(cl & 0xFF);
    dotdot[27] = (unsigned char)((cl >> 8) & 0xFF);

    long offset = cluster_to_offset(fs, new_cluster);
    if (fseek(fs->image, offset, SEEK_SET) == 0) {
        fwrite(buf, 1, cluster_size, fs->image);
    }

    free(buf);
}

static int dir_scan_for_entry(FileSystem *fs,
                              uint32_t dir_cluster,
                              const char short_name[11],
                              long *out_free_offset,
                              int *out_exists) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size =
        bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)malloc(cluster_size);
    if (!buf) return -1;

    long dir_offset = cluster_to_offset(fs, dir_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0) {
        free(buf);
        return -1;
    }

    if (fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        free(buf);
        return -1;
    }

    *out_free_offset = -1;
    *out_exists = 0;

    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        /* 0x00 means this and all following entries are free */
        if (entry[0] == 0x00) {
            if (*out_free_offset == -1) {
                *out_free_offset = dir_offset + (long)off;
            }
            break;
        }

        /* 0xE5 means deleted */
        if (entry[0] == 0xE5) {
            if (*out_free_offset == -1) {
                *out_free_offset = dir_offset + (long)off;
            }
            continue;
        }

        /* Skip long name entries (attribute 0x0F) */
        unsigned char attr = entry[11];
        if ((attr & 0x0F) == 0x0F) {
            continue;
        }

        /* Compare short name */
        if (memcmp(entry, short_name, 11) == 0) {
            *out_exists = 1;
        }
    }

    free(buf);
    return 0;
}

/* write_directory_entry()
 * Writes a single 32-byte FAT directory entry.
 * The entry includes:
 *  - 11-byte short name (uppercased, padded with spaces)
 *  - attribute byte
 *  - first cluster (split between offset 20–21 and 26–27)
 *  - file size (only meaningful for regular files)
 *
 * Used by both fs_mkdir() and fs_creat().
 */
static void write_directory_entry(FileSystem *fs,
                                  long entry_offset,
                                  const char short_name[11],
                                  uint8_t attr,
                                  uint32_t first_cluster,
                                  uint32_t file_size) {
    unsigned char entry[32];
    memset(entry, 0, sizeof(entry));
    memcpy(entry, short_name, 11);
    entry[11] = attr;

    /* First cluster split into high and low parts */
    entry[20] = (unsigned char)((first_cluster >> 24) & 0xFF);
    entry[21] = (unsigned char)((first_cluster >> 16) & 0xFF);
    entry[26] = (unsigned char)(first_cluster & 0xFF);
    entry[27] = (unsigned char)((first_cluster >> 8) & 0xFF);

    /* File size (for regular files) */
    entry[28] = (unsigned char)(file_size & 0xFF);
    entry[29] = (unsigned char)((file_size >> 8) & 0xFF);
    entry[30] = (unsigned char)((file_size >> 16) & 0xFF);
    entry[31] = (unsigned char)((file_size >> 24) & 0xFF);

    if (fseek(fs->image, entry_offset, SEEK_SET) != 0) {
        return;
    }
    
    size_t written = fwrite(entry, sizeof(entry), 1, fs->image);
    if (written != 1) {
        fprintf(stderr, "fwrite failed to write directory entry\n");
    }

}

/* fs_mkdir()
 * Implements FAT32 directory creation:
 *   1. Validate input (1–11 char FAT short names only)
 *   2. Convert to 11-byte short name
 *   3. Scan the current directory cluster for:
 *         - duplicate name
 *         - free entry slot
 *   4. Allocate a free cluster for new directory
 *   5. Initialize '.' and '..' entries in the new cluster
 *   6. Write a directory entry in the parent directory
 *
 * Returns true on success, false on failure.
 */
bool fs_mkdir(FileSystem *fs, const char *name) {
    if (!name || name[0] == '\0') {
        printf("Error: mkdir requires a directory name\n");
        return false;
    }

    size_t len = strlen(name);
    if (len == 0 || len > 11) {
        printf("Error: DIRNAME must be 1–11 characters\n");
        return false;
    }

    /* Build FAT short name */
    char short_name[11];
    build_short_name(short_name, name);

    long free_offset;
    int exists;
    if (dir_scan_for_entry(fs, fs->cwd_cluster, short_name,
                           &free_offset, &exists) != 0) {
        printf("Error: failed to read directory\n");
        return false;
    }

    if (exists) {
        printf("Error: directory/file '%s' already exists\n", name);
        return false;
    }

    if (free_offset == -1) {
        printf("Error: directory is full, cannot create '%s'\n", name);
        return false;
    }

    /* Allocate a new cluster for the directory */
    uint32_t new_cluster = allocate_cluster(fs);
    if (new_cluster == 0) {
        printf("Error: no free clusters available\n");
        return false;
    }

    /* Initialize the new directory cluster with '.' and '..' */
    init_directory_cluster(fs, new_cluster, fs->cwd_cluster);

    /* Create directory entry in current directory */
    write_directory_entry(fs, free_offset, short_name,
                          0x10, /* directory attribute */
                          new_cluster,
                          0 /* size */);

    printf("writing to offset %li" , free_offset);

    return true;
}

/* fs_creat()
 * Creates an **empty file** with size = 0 and first_cluster = 0.
 * No cluster is allocated until the file is written to (Part 4).
 *
 * Steps mirror fs_mkdir(), except:
 *   - attribute = 0x20 (archive / file)
 *   - no '.' or '..' initialization
 *   - first cluster = 0
 */
bool fs_creat(FileSystem *fs, const char *name) {
    if (!name || name[0] == '\0') {
        printf("Error: creat requires a file name\n");
        return false;
    }

    size_t len = strlen(name);
    if (len == 0 || len > 11) {
        printf("Error: FILENAME must be 1–11 characters\n");
        return false;
    }

    char short_name[11];
    build_short_name(short_name, name);

    long free_offset;
    int exists;
    if (dir_scan_for_entry(fs, fs->cwd_cluster, short_name,
                           &free_offset, &exists) != 0) {
        printf("Error: failed to read directory\n");
        return false;
    }

    if (exists) {
        printf("Error: directory/file '%s' already exists\n", name);
        return false;
    }

    if (free_offset == -1) {
        printf("Error: directory is full, cannot create '%s'\n", name);
        return false;
    }

    /* For creat, do NOT allocate a data cluster yet: size = 0, first_cluster = 0 */
    write_directory_entry(fs, free_offset, short_name,
                          0x20, 
                          0,    
                          0);  

    return true;
}

