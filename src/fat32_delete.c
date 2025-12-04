#define _POSIX_C_SOURCE 200809L
#include "fat32.h"
#include "fat32_internal.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * fat32_delete.c
 * Part 6: Delete Commands (rm, rmdir)
 */

/* =============================================================================
 * rm command
 * ============================================================================= */

/*
 * fs_rm()
 * Deletes a file from the current working directory.
 */
bool fs_rm(FileSystem *fs, char *filename, struct OpenFiles *open_files , char* cwd) {

    if (!filename || filename[0] == '\0') {
        printf("Error: rm requires a filename\n");
        return false;
    }

    size_t len = strlen(filename);
    if (len == 0 || len > 11) {
        printf("Error: FILENAME must be 1-11 characters\n");
        return false;
    }

    /* Convert to FAT short name */
    char short_name[11];
    build_short_name(short_name, filename);

    /* Find the directory entry */
    uint32_t start_cluster;
    uint8_t attr;
    long entry_offset = find_directory_entry_offset(fs, short_name, &start_cluster, &attr);

    if (entry_offset == -1) {
        printf("Error: file '%s' does not exist\n", filename);
        return false;
    }

    /* Check if it's a directory */
    if (attr & 0x10) {
        printf("Error: '%s' is a directory\n", filename);
        return false;
    }

    /* Check if file is open */
    if (checkIsOpen(start_cluster, open_files , cwd , filename ) == -1) {
        printf("Error: file '%s' is currently open\n", filename);
        return false;
    }

    /* Mark directory entry as deleted (0xE5) */
    unsigned char deleted_marker = 0xE5;
    if (fseek(fs->image, entry_offset, SEEK_SET) != 0) {
        printf("Error: failed to seek to directory entry\n");
        return false;
    }
    if (fwrite(&deleted_marker, 1, 1, fs->image) != 1) {
        printf("Error: failed to mark entry as deleted\n");
        return false;
    }

    /* Free all clusters in the cluster chain */
    if (start_cluster >= 2) {
        free_cluster_chain(fs, start_cluster);
    }

    /* Flush changes to disk */
    fflush(fs->image);

    return true;
}

/* =============================================================================
 * rmdir command
 * ============================================================================= */

/*
 * fs_rmdir()
 * Removes a directory from the current working directory.
 *
 */
bool fs_rmdir(FileSystem *fs, const char *dirname, struct OpenFiles *open_files) {
    if (!dirname || dirname[0] == '\0') {
        printf("Error: rmdir requires a directory name\n");
        return false;
    }

    size_t len = strlen(dirname);
    if (len == 0 || len > 11) {
        printf("Error: DIRNAME must be 1-11 characters\n");
        return false;
    }

    /* Convert to FAT short name */
    char short_name[11];
    build_short_name(short_name, dirname);

    /* Find the directory entry */
    uint32_t start_cluster;
    uint8_t attr;
    long entry_offset = find_directory_entry_offset(fs, short_name, &start_cluster, &attr);

    if (entry_offset == -1) {
        printf("Error: directory '%s' does not exist\n", dirname);
        return false;
    }

    /* Check if it's not a directory */
    if (!(attr & 0x10)) {
        printf("Error: '%s' is not a directory\n", dirname);
        return false;
    }

    /* Check if directory is empty */
    if (!is_directory_empty(fs, start_cluster)) {
        printf("Error: directory '%s' is not empty\n", dirname);
        return false;
    }

    /* Check if any files in the directory are open
     * We need to check all open files to see if their path starts with this directory */
    for (int i = 0; i < 10; i++) {
        if (open_files->files[i].open == 1) {
            /* Get current directory path */
            CurrentDirectory cwd = getcwd(fs);

            /* Build the path to this directory */
            char dir_path[512];
            snprintf(dir_path, sizeof(dir_path), "%s/%s", cwd.cwd, dirname);

            /* Check if any open file's path starts with this directory path */
            if (strncmp(open_files->files[i].filePath, dir_path, strlen(dir_path)) == 0) {
                printf("Error: directory '%s' contains an open file\n", dirname);
                free(cwd.cwd);

                return false;
            }

            free(cwd.cwd);
        }
    }

    /* Mark directory entry as deleted (0xE5) */
    unsigned char deleted_marker = 0xE5;

    if (fseek(fs->image, entry_offset, SEEK_SET) != 0) {
        printf("Error: failed to seek to directory entry\n");
        return false;
    }
    if (fwrite(&deleted_marker, 1, 1, fs->image) != 1) {
        printf("Error: failed to mark entry as deleted\n");
        return false;
    }

    /* Free the directory's cluster */
    if (start_cluster >= 2) {
        free_cluster_chain(fs, start_cluster);
    }

    fflush(fs->image); //force

    return true;
}
