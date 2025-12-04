#define _POSIX_C_SOURCE 200809L
#include "fat32.h"
#include "fat32_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * fat32_read.c
 * Part 4: Read Operations (file existence checks, reading data)
 */

/* =============================================================================
 * File existence and metadata checks
 * ============================================================================= */

/* checkExists()
 * Returns 0 if a file/directory with filename exists in the current
 * working directory (fs->cwd_cluster). Returns -1 on error or
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
 *'filename' in the current working directory. Returns 0 if not found or on error.
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

/* getFileSize()
 * Returns the size (in bytes) of the file with name "filename" in the current
 * working directory of 'fs'. Assumes the file exists. Returns 0 if not found
 * or on error.
 */
uint32_t getFileSize(char* filename, FileSystem* fs) {

    if (!filename || !fs || !fs->image)
        return 0;

    char short_name[11];

    build_short_name(short_name, filename);

    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *) malloc(cluster_size);

    if (!buf)
        return 0;

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);

    if (fseek(fs->image, dir_offset, SEEK_SET) != 0 ||
        fread(buf, 1, cluster_size, fs->image) != cluster_size) {

        free(buf);
        return 0;
    }

    //Scan for the entry
    for (uint32_t off = 0; off < cluster_size; off += 32) {

        unsigned char* entry = buf + off;

        if (entry[0] == 0x00)
            break;
        if (entry[0] == 0xE5)
            continue;

        unsigned char attr = entry[11];

        if ((attr & 0x0F) == 0x0F)
            continue;

        //check if name matches
        if (memcmp(entry, short_name, 11) == 0) {
            //file size is stored at bytes 28-31

            uint32_t file_size = read_le32( entry + 28 );

            free(buf);
            return file_size;
        }
    }

    free(buf);

    return 0;
}

/* =============================================================================
 * File reading
 * ============================================================================= */

/* readFile()
 * reads from filename in cwd, 0 on error or none read
 */
uint32_t readFile(uint32_t start_offset, uint32_t size_to_read, char* filename, FileSystem* fs) {

    if (!filename || !fs || !fs->image)
        return 0;

    uint32_t file_size = getFileSize(filename, fs);

    if (start_offset >= file_size)
        return 0;

    uint32_t remaining = file_size - start_offset;
    uint32_t to_read = (size_to_read < remaining) ? size_to_read : remaining;

    if (to_read == 0)
        return 0;

    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;


    uint32_t cur_cluster = getStartCluster(filename, fs);

    if (cur_cluster == 0)
        return 0;

    //Advance to the cluster that contains start_offset
    uint32_t cluster_index = start_offset / cluster_size;
    uint32_t offset_in_cluster = start_offset % cluster_size;

    for (uint32_t i = 0; i < cluster_index; i++ ) {

        uint32_t next = read_fat_entry(fs, cur_cluster);

        if (next >= FAT32_EOC) {
            return 0;
        }

        cur_cluster = next;
    }

    unsigned char *buf = (unsigned char*) malloc(cluster_size);

    if (!buf)
        return 0;

    uint32_t bytes_read = 0;

    while (bytes_read < to_read) {

        long cluster_off = cluster_to_offset(fs, cur_cluster);

        if (fseek(fs->image, cluster_off + offset_in_cluster, SEEK_SET) != 0)
            break;

        uint32_t can_read = cluster_size - offset_in_cluster;

        uint32_t want = to_read - bytes_read;

        uint32_t n = (want < can_read) ? want : can_read;

        if (fread(buf, 1, n, fs->image) != n)
            break;


        size_t written = fwrite(buf, 1, n, stdout);
        (void) written;

        bytes_read += n;

        offset_in_cluster = 0;

        if (bytes_read < to_read) {

            uint32_t next = read_fat_entry(fs, cur_cluster);

            if (next >= FAT32_EOC)
                break;

            cur_cluster = next;
        }
    }

    free(buf);
    return bytes_read;
}
