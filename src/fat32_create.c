#define _POSIX_C_SOURCE 200809L
#include "fat32.h"
#include "fat32_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * fat32_create.c
 * Part 3: Create Commands (mkdir, creat)
 */

/* =============================================================================
 * mkdir command
 * ============================================================================= */

/* fs_mkdir()
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

    return true;
}

/* =============================================================================
 * creat command
 * ============================================================================= */

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
