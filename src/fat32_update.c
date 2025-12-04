#define _POSIX_C_SOURCE 200809L
#include "fat32.h"
#include "fat32_internal.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * fat32_update.c
 * Part 5: Update Commands (write, mv)
 */

/* =============================================================================
 * write command
 * ============================================================================= */

/* writeToFile()
 * writes the bytes to filename
 * returns the number of bytes written or 0 on error or none.
 */
uint32_t writeToFile(const char* filename, const char* bytes_to_write, uint32_t start_offset, FileSystem* fs , OpenFile* file ) {

    if (!filename || !bytes_to_write || !fs || !fs->image)
        return 0;

    size_t write_len = strlen(bytes_to_write);

    if (write_len == 0)
        return 0;

    char short_name[11];
    build_short_name(short_name, filename);

    uint32_t start_cluster = 0;
    uint8_t attr = 0;

    long entry_offset = find_directory_entry_offset(fs, short_name, &start_cluster, &attr);

    if (entry_offset == -1) //silly kid
        return 0;

    if (attr & 0x10) //dir check
        return 0;

    uint32_t old_size = getFileSize((char*)filename, fs);

    //bound to EOF
    uint32_t write_offset = (start_offset > old_size) ? old_size : start_offset;

    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    //get cluster or make one
    uint32_t cur_cluster = start_cluster;

    if (cur_cluster == 0) {

        uint32_t new_cluster = allocate_cluster(fs);

        if (new_cluster == 0)
            return 0;

        cur_cluster = new_cluster;
        start_cluster = new_cluster;
    }

    file->startCluster = cur_cluster; //set new start cluster for id and usage

    //get cluster range
    uint32_t cluster_count = 1;
    uint32_t last_cluster = cur_cluster;

    while (true) {

        uint32_t next = read_fat_entry(fs, last_cluster);

        if (next >= FAT32_EOC)
            break;

        last_cluster = next;
        cluster_count++;

        if (cluster_count > fs->total_clusters + 7)
            break;
    }

    //calc cluster size increaase needed?
    uint32_t final_needed_size = write_offset + (uint32_t)write_len;
    uint32_t required_clusters = (final_needed_size + cluster_size - 1) / cluster_size;

    if (required_clusters > cluster_count) {

        uint32_t need = required_clusters - cluster_count;

        for (uint32_t i = 0; i < need; i++ ) {

            uint32_t new_cluster = allocate_cluster(fs);

            if (new_cluster == 0)
                return 0;

            write_fat_entry(fs, last_cluster, new_cluster);

            write_fat_entry(fs, new_cluster, FAT32_EOC);

            last_cluster = new_cluster;
            ++cluster_count;
        }
    }

    //find cluster that contains write_offset
    uint32_t target_cluster = start_cluster;

    uint32_t skip_clusters = write_offset / cluster_size;

    uint32_t off_in_cluster = write_offset % cluster_size;

    for (uint32_t i = 0; i < skip_clusters; i++ ) {

        uint32_t next = read_fat_entry(fs, target_cluster);

        if (next >= FAT32_EOC)
            break;

        target_cluster = next;
    }

    const char *src = bytes_to_write;

    uint32_t remaining = (uint32_t) write_len;

    uint32_t written = 0;

    unsigned char *buf = (unsigned char*) malloc(cluster_size);

    if (!buf)
        return 0;

    while (remaining > 0) {

        long cluster_off = cluster_to_offset(fs, target_cluster);

        if (fseek(fs->image, cluster_off + off_in_cluster, SEEK_SET) != 0)
            break;

        uint32_t can = cluster_size - off_in_cluster;
        uint32_t to_write = (remaining < can) ? remaining : can;

        if (fwrite(src, 1, to_write, fs->image) != to_write)
            break;

        written += to_write;
        remaining -= to_write;
        src += to_write;

        off_in_cluster = 0;

        if (remaining > 0) {

            uint32_t next = read_fat_entry(fs, target_cluster);

            if (next >= FAT32_EOC)
                break;

            target_cluster = next;
        }
    }

    free(buf);

    // update directory entry, first cluster (if changed) and file size
    unsigned char entry[32];
    if (fseek(fs->image, entry_offset, SEEK_SET) != 0)
        return written;
    if (fread(entry, 1, 32, fs->image) != 32)
        return written;

    entry[20] = (unsigned char)((start_cluster >> 16) & 0xFF);
    entry[21] = (unsigned char)((start_cluster >> 24) & 0xFF);
    entry[26] = (unsigned char)(start_cluster & 0xFF);
    entry[27] = (unsigned char)((start_cluster >> 8) & 0xFF);

    uint32_t final_size = (old_size > (write_offset + written)) ? old_size : (write_offset + written);

    entry[28] = (unsigned char)(final_size & 0xFF);
    entry[29] = (unsigned char)((final_size >> 8) & 0xFF);
    entry[30] = (unsigned char)((final_size >> 16) & 0xFF);
    entry[31] = (unsigned char)((final_size >> 24) & 0xFF);

    if (fseek(fs->image, entry_offset, SEEK_SET) == 0) {
        fwrite(entry, 1, 32, fs->image);
    }

    fflush(fs->image);

    return written;
}

/* =============================================================================
 * mv command
 * ============================================================================= */

/*
* fs_mv()
* moves a file, returns false on failure and may print an error message
*/
bool fs_mv(FileSystem *fs, char *src, char *dest, struct OpenFiles *open_files, CurrentDirectory *cwd_info)
{
    if (!fs || !src || !dest) {
        printf("Error: invalid arguments to mv\n");
        return false;
    }

    /* Build short names for FAT directory entries */
    char src_short[11];
    char dest_short[11];

    build_short_name(src_short, src);
    build_short_name(dest_short, dest);

    /* Find source entry in current directory */
    uint32_t src_cluster = 0;
    uint8_t  src_attr    = 0;
    long src_offset = find_directory_entry_offset(fs, src_short, &src_cluster, &src_attr);

    if (src_offset < 0) {
        printf("Error: source '%s' does not exist\n", src);
        return false;
    }

    /* file must be closed */
    uint32_t start_cluster = getStartCluster(src_short, fs);
    if (start_cluster != 0 &&
        checkIsOpen(start_cluster, open_files, cwd_info->cwd, src) != 0) {
        printf("Error: '%s' is currently open; close it before mv\n", src);
        return false;
    }

    /* Check if dest is an existing entry in current directory */
    uint32_t dest_cluster = 0;
    uint8_t  dest_attr    = 0;
    long dest_offset = find_directory_entry_offset(fs, dest_short, &dest_cluster, &dest_attr);

    /* Reject moving directories*/
    if (src_attr & 0x10) {   // 0x10 = directory attribute
        printf("Error: cannot mv a directory\n");
        return false;
    }

    /* Case 1: dest exists and is a directory , move into that directory
       (we keep the original name). */
    if (dest_offset >= 0 && (dest_attr & 0x10)) {
        uint32_t target_dir_cluster = getStartCluster(dest_short, fs);

        if (target_dir_cluster == 0) {
            printf("Error: failed to resolve destination directory '%s'\n", dest);
            return false;
        }

        /* Read the source directory entry */
        unsigned char entry[32];
        if (fseek(fs->image, src_offset, SEEK_SET) != 0 ||
            fread(entry, 1, 32, fs->image) != 32) {
            printf("Error: failed to read source directory entry\n");
            return false;
        }

        /* Find a free slot in the destination directory */
        long free_offset;
        int exists;

        /* scan for free slot in destination directory */
        if (dir_scan_for_entry(fs, target_dir_cluster, "           ", &free_offset, &exists) != 0) {
            printf("Error: directory scan failed for destination\n");
            return false;
        }

        if (exists) {
            printf("Error: unexpected duplicate during free-scan\n");
            return false;
        }

        if (free_offset < 0) {
            printf("Error: destination directory full\n");
            return false;
        }

        /* Write the copied entry into the destination directory */
        if (fseek(fs->image, free_offset, SEEK_SET) != 0 ||
            fwrite(entry, 1, 32, fs->image) != 32) {
            printf("Error: failed to write directory entry in destination\n");
            return false;
        }

        /* Mark old entry as free (0xE5 in first byte) */
        if (fseek(fs->image, src_offset, SEEK_SET) == 0) {
            unsigned char del = 0xE5;
            fwrite(&del, 1, 1, fs->image);
        }

        fflush(fs->image);
        return true;
    }

    /* Case 2: dest does NOT exist -> simple rename in current directory. */
    if (dest_offset < 0) {
        unsigned char entry[32];

        if (fseek(fs->image, src_offset, SEEK_SET) != 0 ||
            fread(entry, 1, 32, fs->image) != 32) {
            printf("Error: failed to read source directory entry\n");
            return false;
        }

        /* Overwrite the name field with new short name */
        memcpy(entry, dest_short, 11);

        if (fseek(fs->image, src_offset, SEEK_SET) != 0 ||
            fwrite(entry, 1, 32, fs->image) != 32) {
            printf("Error: failed to write renamed directory entry\n");
            return false;
        }

        fflush(fs->image);
        return true;
    }

    /* Case 3: dest exists but is NOT a directory -> error. */
    printf("Error: destination '%s' is a file, not a directory\n", dest);
    return false;
}
