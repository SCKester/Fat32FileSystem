#define _POSIX_C_SOURCE 200809L
#include "fat32.h"
#include "fat32_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * fat32_cluster.c
 * Low-level cluster and FAT table operations
 */

/* =============================================================================
 * Little-endian readers
 * ============================================================================= */

uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* =============================================================================
 * Cluster operations
 * ============================================================================= */

/* Convert a cluster number to a byte offset in the image file */
long cluster_to_offset(const FileSystem *fs, uint32_t cluster) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t bytes_per_sector    = bpb->bytes_per_sector;
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;

    uint32_t first_data_sector = fs->first_data_sector;
    uint32_t sector =
        first_data_sector + (cluster - 2) * sectors_per_cluster;

    return (long)sector * bytes_per_sector;
}

/* =============================================================================
 * FAT table operations
 * ============================================================================= */

/* Read a FAT32 entry for a given cluster.
 *
 * Each FAT32 entry is 32 bits.
 * FAT offset = cluster * 4
 * FAT sector = fat_start_sector + (fat_offset / bytes_per_sector)
 * entry offset = fat_offset % bytes_per_sector
 *
 * function returns:
 *   - next cluster in chain
 *   - FAT32_EOC when at end of cluster chain
 */
uint32_t read_fat_entry(FileSystem *fs, uint32_t cluster) {
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
void write_fat_entry(FileSystem *fs, uint32_t cluster, uint32_t value) {
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
uint32_t allocate_cluster(FileSystem *fs) {
    for (uint32_t c = 2; c < fs->total_clusters + 2; c++) {
        uint32_t val = read_fat_entry(fs, c);
        if (val == 0x00000000) {
            write_fat_entry(fs, c, FAT32_EOC);
            return c;
        }
    }
    return 0; /* No free cluster */
}

/* Free all clusters in a cluster chain by marking them as free (0x00000000) in the FAT */
void free_cluster_chain(FileSystem *fs, uint32_t start_cluster) {
    uint32_t cluster = start_cluster;

    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next_cluster = read_fat_entry(fs, cluster);
        write_fat_entry(fs, cluster, 0x00000000);
        cluster = next_cluster;
    }
}
