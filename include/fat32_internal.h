#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "fat32.h"

/*
 * fat32_internal.h
 * Internal helper functions shared between FAT32 modules.
 * These are NOT part of the public API.
 */

/* =============================================================================
 * Little-endian readers (used by multiple modules)
 * ============================================================================= */
uint16_t read_le16(const unsigned char *p);
uint32_t read_le32(const unsigned char *p);

/* =============================================================================
 * Cluster and FAT operations (fat32_cluster.c)
 * ============================================================================= */

/* Convert cluster number to byte offset in image file */
long cluster_to_offset(const FileSystem *fs, uint32_t cluster);

/* Read a FAT32 entry for a given cluster */
uint32_t read_fat_entry(FileSystem *fs, uint32_t cluster);

/* Write a FAT32 entry for a given cluster */
void write_fat_entry(FileSystem *fs, uint32_t cluster, uint32_t value);

/* Find and allocate a free cluster */
uint32_t allocate_cluster(FileSystem *fs);

/* Free all clusters in a cluster chain */
void free_cluster_chain(FileSystem *fs, uint32_t start_cluster);

/* FAT32 End of Cluster chain marker */
#define FAT32_EOC 0x0FFFFFFF

/* =============================================================================
 * Helper functions (fat32_helpers.c)
 * ============================================================================= */

/* Build FAT short name from a regular name string */
void build_short_name(char dest[11], const char *name);

/* Scan directory for entry, find free slot */
int dir_scan_for_entry(FileSystem *fs, uint32_t dir_cluster,
                       const char short_name[11],
                       long *out_free_offset, int *out_exists);

/* Write a directory entry to the specified offset */
void write_directory_entry(FileSystem *fs, long entry_offset,
                           const char short_name[11], uint8_t attr,
                           uint32_t first_cluster, uint32_t file_size);

/* Initialize a new directory cluster with . and .. entries */
void init_directory_cluster(FileSystem *fs, uint32_t new_cluster,
                            uint32_t parent_cluster);

/* Find directory entry and return its offset, cluster, and attributes */
long find_directory_entry_offset(FileSystem *fs, const char short_name[11],
                                 uint32_t *out_cluster, uint8_t *out_attr);

/* Check if a directory is empty (only . and .. entries) */
bool is_directory_empty(FileSystem *fs, uint32_t dir_cluster);
