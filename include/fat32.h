#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* FAT32 boot sector struct */
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint32_t fat_size_sectors;
    uint32_t total_sectors;
    uint32_t root_cluster;
} Fat32BootSector;

/* Represents mounted FAT32 filesystem */
typedef struct {
    FILE *image;
    char  image_name[256];
    Fat32BootSector bpb;
        /* Sector index where the first FAT starts */
    uint32_t fat_start_sector;

    /* Sector index where the data region (cluster 2) starts */
    uint32_t first_data_sector;

    /* Total number of clusters in the data region */
    uint32_t total_clusters;

    /* Current working directory cluster (initialized to root) */
    uint32_t cwd_cluster;
} FileSystem;

/* Mount/unmount functions */
bool fs_mount(FileSystem *fs, const char *image_path);
void fs_unmount(FileSystem *fs);

/* Commands for Part 1 */
void cmd_info(const FileSystem *fs);

/* Create a new directory in the current working directory */
bool fs_mkdir(FileSystem *fs, const char *name);

/* Create a new empty file (size 0) in the current working directory */
bool fs_creat(FileSystem *fs, const char *name);