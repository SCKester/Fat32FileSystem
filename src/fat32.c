#define _POSIX_C_SOURCE 200809L
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

    /* Cluster number for "." (little-endian) */
    uint32_t cl = new_cluster;
    dot[20] = (unsigned char)((cl >> 16) & 0xFF);
    dot[21] = (unsigned char)((cl >> 24) & 0xFF);
    dot[26] = (unsigned char)(cl & 0xFF);
    dot[27] = (unsigned char)((cl >> 8) & 0xFF);

    /* ".." entry (parent) */
    unsigned char *dotdot = buf + 32;
    memset(dotdot, ' ', 11);
    dotdot[0] = '.';
    dotdot[1] = '.';
    dotdot[11] = 0x10; 

    cl = parent_cluster;
    dotdot[20] = (unsigned char)((cl >> 16) & 0xFF);
    dotdot[21] = (unsigned char)((cl >> 24) & 0xFF);
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

    /* First cluster split into high and low parts (little-endian) */
    entry[20] = (unsigned char)((first_cluster >> 16) & 0xFF);
    entry[21] = (unsigned char)((first_cluster >> 24) & 0xFF);
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

/*
 * Creates an empty file with size = 0 and allocates a starting cluster.
 *  attribute = 0x20
 *  ALLOCATES a starting cluster for the file
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

    //Allocate a starting cluster for the file
    uint32_t start_cluster = allocate_cluster(fs);

    if (start_cluster == 0) {
        printf("Error: no free clusters available\n");
        return false;
    }

    //Write directory entry with the allocated cluster
    write_directory_entry(fs, free_offset, short_name,
                          0x20,
                          start_cluster,
                          0);

    return true;
}

/* 
 * Lists all directory entries in the current working directory.
 * Prints the name field for each directory and file, including "." and "..".
 * Skips deleted entries (0xE5) and long filename entries (0x0F).
 */
void fs_ls(const FileSystem *fs) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)malloc(cluster_size);
    if (!buf) {
        printf("Error: memory allocation failed\n");
        return;
    }

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0) {
        printf("Error: failed to seek to directory cluster\n");
        free(buf);
        return;
    }

    if (fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        printf("Error: failed to read directory cluster\n");
        free(buf);
        return;
    }

    /* Iterate through all 32-byte directory entries */
    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        /* 0x00 means this and all following entries are free */
        if (entry[0] == 0x00) {
            break;
        }

        /* 0xE5 means deleted, skip */
        if (entry[0] == 0xE5) {
            continue;
        }

        /* Skip long filename entries (attribute 0x0F) */
        unsigned char attr = entry[11];
        if ((attr & 0x0F) == 0x0F) {
            continue;
        }

        /* Extract and print the 11-byte short name */
        char name[12];
        memcpy(name, entry, 11);
        name[11] = '\0';

        /* Print the name (it will have trailing spaces, but that's okay) */
        printf("%s\n", name);
    }

    free(buf);
}

/* 
 * Changes the current working directory to DIRNAME.
 * Returns true on success, false on failure.
 * Prints an error message if DIRNAME does not exist or is not a directory.
 */
bool fs_cd(FileSystem *fs, const char *dirname) {
    if (!dirname || dirname[0] == '\0') {
        printf("Error: cd requires a directory name\n");
        return false;
    }

    /* Build FAT short name */
    char short_name[11];
    build_short_name(short_name, dirname);

    /* Read directory entries to find the target */
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)malloc(cluster_size);
    if (!buf) {
        printf("Error: memory allocation failed\n");
        return false;
    }

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0) {
        printf("Error: failed to seek to directory cluster\n");
        free(buf);
        return false;
    }

    if (fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        printf("Error: failed to read directory cluster\n");
        free(buf);
        return false;
    }

    /* Search for the directory entry */
    bool found = false;
    uint32_t target_cluster = 0;

    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        /* 0x00 means this and all following entries are free */
        if (entry[0] == 0x00) {
            break;
        }

        /* 0xE5 means deleted, skip */
        if (entry[0] == 0xE5) {
            continue;
        }

        /* Skip long filename entries (attribute 0x0F) */
        unsigned char attr = entry[11];
        if ((attr & 0x0F) == 0x0F) {
            continue;
        }

        /* Compare short name */
        if (memcmp(entry, short_name, 11) == 0) {
            /* Check if it's a directory (attribute 0x10) */
            if ((attr & 0x10) == 0) {
                printf("Error: '%s' is not a directory\n", dirname);
                free(buf);
                return false;
            }

            /* Extract the cluster number from the entry */
            /* Cluster is stored as: high word at bytes 20-21, low word at bytes 26-27 (little-endian) */
            target_cluster = ((uint32_t)entry[21] << 24) | ((uint32_t)entry[20] << 16) |
                           ((uint32_t)entry[27] << 8) | (uint32_t)entry[26];

            found = true;
            break;
        }
    }

    free(buf);

    if (!found) {
        printf("Error: '%s' does not exist\n", dirname);
        return false;
    }

    /* In FAT32, if target cluster is 0, it means the root directory */
    if (target_cluster == 0) {
        target_cluster = fs->bpb.root_cluster;
    }

    /* Update current working directory */
    fs->cwd_cluster = target_cluster;
    return true;
}

/* getcwd()
 * Builds a full path from the current working directory by walking up
 * the directory tree following the ".." entries until the root cluster.
 * Returns a dynamically allocatew c string and its size ( caller must free c string ) containing the
 * path in the form "/dir1/dir2". root returns "/".
*/

CurrentDirectory getcwd( FileSystem *fs ) {

    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t root = bpb->root_cluster;
    uint32_t cur = fs->cwd_cluster;
    CurrentDirectory directory;

    directory.size = -1; //err

    /* If at root, return "/" */
    if (cur == root) {
        char* s = (char*) malloc(2);

        if (s) { s[0] = '/'; s[1] = '\0'; }
        
        directory.size = 2;

        directory.cwd = s;

        return directory;
    }

    /* Collect path components (child -> parent). We'll store pointers in a
     * dynamic array and later join them in reverse order. */
    size_t cap = 8;
    size_t nseg = 0;
    char** segments = (char**) malloc(cap * sizeof(char*));

    while (cur != root) {
        // Read current directory cluster to find ".." entry (parent) 

        const uint32_t cluster = cur;

        uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

        unsigned char* buf = (unsigned char*) malloc(cluster_size);

        if (!buf) break;

        long dir_offset = cluster_to_offset(fs, cluster);
        if (fseek(fs->image, dir_offset, SEEK_SET) != 0 ||
            fread(buf, 1, cluster_size, fs->image) != cluster_size) {
            free(buf);
            break;
        }

        uint32_t parent = 0;
        // Find the entry for ".." 
        for (uint32_t off = 0; off < cluster_size; off += 32) {

            unsigned char *entry = buf + off;

            if (entry[0] == 0x00) break;

            if (entry[0] == 0xE5) continue;

            unsigned char attr = entry[11];

            if ((attr & 0x0F) == 0x0F) continue; // long name 

            if (entry[0] == '.' && entry[1] == '.') {
                parent = ((uint32_t)entry[21] << 24) | ((uint32_t)entry[20] << 16) |
                         ((uint32_t)entry[27] << 8) | (uint32_t)entry[26];
                break;
            }
        }

        free(buf);

        if (parent == 0) {  
            parent = root;
        }
        /* Now scan parent directory to find the entry whose cluster == cur
         * That entry's name is the segment we need to prepend. */
        cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
        buf = (unsigned char*)malloc(cluster_size);
        if (!buf) break;

        dir_offset = cluster_to_offset(fs, parent);
        if (fseek(fs->image, dir_offset, SEEK_SET) != 0 ||
            fread(buf, 1, cluster_size, fs->image) != cluster_size) {
            free(buf);
            break;
        }

        char found_name[13];
        bool found = false;

        for (uint32_t off = 0; off < cluster_size; off += 32) {
            unsigned char *entry = buf + off;
            if (entry[0] == 0x00) break;
            if (entry[0] == 0xE5) continue;
            unsigned char attr = entry[11];
            if ((attr & 0x0F) == 0x0F) continue; // long name

            // Extract cluster of this entry 
            uint32_t ent_cluster =
                ((uint32_t)entry[21] << 24) |
                ((uint32_t)entry[20] << 16) |
                ((uint32_t)entry[27] <<  8) |
                (uint32_t)entry[26];

            if (ent_cluster == cur) {

                char name_part[9];
                char ext_part[4];
                memcpy(name_part, entry, 8);
                name_part[8] = '\0';
                memcpy(ext_part, entry + 8, 3);
                ext_part[3] = '\0';

                for (int i = 7; i >= 0; --i) {
                    if (name_part[i] == ' ') name_part[i] = '\0'; else break;
                }
                for (int i = 2; i >= 0; --i) {
                    if (ext_part[i] == ' ') ext_part[i] = '\0'; else break;
                }

                if (ext_part[0] != '\0') {
                    snprintf(found_name, sizeof(found_name), "%s.%s", name_part, ext_part);
                } else {
                    snprintf(found_name, sizeof(found_name), "%s", name_part);
                }

                found = true;
                break;
            }
        }

        free(buf);

        if (!found) break;


        if (nseg + 1 > cap) {
            size_t ncap = cap * 2;
            char** tmp = (char**) realloc(segments, ncap * sizeof(char*));
            if (!tmp) break;
            segments = tmp;
            cap = ncap;
        }

        segments[nseg++] = strdup(found_name);


        cur = parent;
    }


    // / as fallback
    if (nseg == 0) {
        free(segments);
        char* s = (char*) malloc(2);
        if (s) { s[0] = '/'; s[1] = '\0'; }

        directory.size = 2;
        directory.cwd = s;

        return directory;
    }

    //compute total length for the joined path: leading '/', segments and '/' between
    size_t total = 1; 

    for (size_t i = 0; i < nseg; ++i) total += strlen(segments[i]) + 1;

    char* path = (char*) malloc(total + 1);
    if (!path) {

        for (size_t i = 0; i < nseg; ++i) 
            free(segments[i]);

        free(segments);

        return directory;
    }

    char* p = path;
    *p++ = '/';

    //segments are from child->parent; we need to print in reverse 
    for (size_t i = 0; i < nseg; ++i) {

        char* seg = segments[nseg - 1 - i];

        size_t len = strlen(seg);

        memcpy(p, seg, len);

        p += len;

        if (i + 1 < nseg) *p++ = '/';

        free(seg);
    }

    *p = '\0';

    free(segments);

    directory.size = total;
    directory.cwd = path;

    return directory;
}

/* checkExists()
 * Returns 0 if a file/directory with `filename` exists in the current
 * working directory (`fs->cwd_cluster`). Returns -1 on error or
 * if it does not exist.
 */
size_t checkExists(char* filename, FileSystem* fs) {
    if (!filename || !fs) 
        return -1;

    char short_name[11];
    build_short_name(short_name, filename);

    long free_offset;
    int exists;

    if (dir_scan_for_entry(fs, fs->cwd_cluster, short_name, &free_offset, &exists) != 0) {
        return -1;
    }

    return exists ? 0 : -1;
}

/* checkIsFile()
 * Returns 0 if the entry with name 'filename' exists in the current working
 * directory and is a regular file (not a directory). returns -1 if
 * it does not exist, is a directory, or on error.
 */
size_t checkIsFile(char* filename, FileSystem* fs) {
    if (!filename || !fs || !fs->image) 
        return -1;

    char short_name[11];
    build_short_name(short_name, filename);

    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *) malloc(cluster_size);

    if (!buf) 
        return -1;

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0 ||
        fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        free(buf);
        return -1;
    }

    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        if (entry[0] == 0x00) 
            break;          
        if (entry[0] == 0xE5) 
            continue;        

        unsigned char attr = entry[11];

        if ((attr & 0x0F) == 0x0F) 
            continue;   

        if ( memcmp( entry, short_name, 11 ) == 0) {

            //directory attribute is 0x10; file is 0x20
            size_t result = ((attr & 0x10) == 0) ? 0 : -1;

            free(buf);
            return result;
        }
    }

    free(buf);
    return -1;  /* Entry not found */
}

/* getStartCluster()
 * returns the starting cluster number of the file/directory entry with name
 * `filename` in the current working directory. Returns 0 if not found or on error.
 */
uint32_t getStartCluster(char* filename, FileSystem* fs) {

    if (!filename || !fs || !fs->image) 
        return 0;

    char short_name[11];
    build_short_name(short_name, filename);

    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)malloc(cluster_size);
    if (!buf) return 0;

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0 ||
        fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        free(buf);
        return 0;
    }

    //Scan for the entry 
    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        if (entry[0] == 0x00) 
            break;           
        if (entry[0] == 0xE5) 
            continue;        

        unsigned char attr = entry[11];
        if ((attr & 0x0F) == 0x0F) 
            continue;  

        //Check if name matches
        if (memcmp(entry, short_name, 11) == 0) {
            uint32_t start_cluster = ((uint32_t)entry[21] << 24) | ((uint32_t)entry[20] << 16) |
                                     ((uint32_t)entry[27] << 8) | (uint32_t)entry[26];

            free(buf);
            return start_cluster;
        }
    }

    free(buf);

    return 0; 
}

