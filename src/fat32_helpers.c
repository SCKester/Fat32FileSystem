#define _POSIX_C_SOURCE 200809L
#include "fat32.h"
#include "fat32_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * fat32_helpers.c
 * Shared utility functions used by multiple FAT32 modules
 */

/* =============================================================================
 * Name handling
 * ============================================================================= */

void build_short_name(char dest[11], const char *name) {
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

/* =============================================================================
 * Directory entry operations
 * ============================================================================= */

/* Scan directory cluster for a specific entry name, and also find first free slot.
 * Returns 0 on success, -1 on error.
 * Sets *out_free_offset to the byte offset of a free slot (or -1 if none).
 * Sets *out_exists to 1 if the entry with short_name already exists, 0 otherwise.
 */
int dir_scan_for_entry(FileSystem *fs,
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

/* Write a single 32-byte FAT directory entry.
 * Used by both mkdir and creat commands.
 */
void write_directory_entry(FileSystem *fs,
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

/* Initialize a newly allocated directory cluster with "." and ".." entries */
void init_directory_cluster(FileSystem *fs,
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

/* Find directory entry and return its byte offset, cluster, and attributes.
 * Returns byte offset of the entry, or -1 if not found.
 */
long find_directory_entry_offset(FileSystem *fs, const char short_name[11],
                                 uint32_t *out_cluster, uint8_t *out_attr) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)malloc(cluster_size);
    if (!buf) return -1;

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0) {
        free(buf);
        return -1;
    }

    if (fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        free(buf);
        return -1;
    }

    long entry_offset = -1;

    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        unsigned char attr = entry[11];

        if ((attr & 0x0F) == 0x0F) continue;

        if (memcmp(entry, short_name, 11) == 0) {
            *out_attr = attr;
            *out_cluster = ((uint32_t)entry[21] << 24) | ((uint32_t)entry[20] << 16) |
                          ((uint32_t)entry[27] << 8) | (uint32_t)entry[26];
            entry_offset = dir_offset + (long)off;
            break;
        }
    }

    free(buf);
    return entry_offset;
}

/* Check if a directory contains only "." and ".." entries.
 * Returns true if empty, false otherwise.
 */
bool is_directory_empty(FileSystem *fs, uint32_t dir_cluster) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *) malloc(cluster_size);

    if (!buf) return false;

    long dir_offset = cluster_to_offset(fs, dir_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0) {
        free(buf);
        return false;
    }

    if (fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        free(buf);
        return false;
    }

    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        unsigned char attr = entry[11];
        if ((attr & 0x0F) == 0x0F) continue;

        /* Skip "." and ".." */
        if (entry[0] == '.' && (entry[1] == ' ' || entry[1] == '.')) {
            continue;
        }

        free(buf);
        return false;
    }

    free(buf);
    return true;
}
