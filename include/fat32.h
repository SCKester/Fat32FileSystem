#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "utils.h"

/*
 * FAT32 Boot Sector 
 */
typedef struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint32_t fat_size_sectors;
    uint32_t total_sectors;
    uint32_t root_cluster;
} Fat32BootSector;

/*
 * FileSystem
 */
typedef struct {
    FILE *image; // handle to opened FAT32 image file
    char  image_name[256];  // name shown in the shell prompt
    Fat32BootSector bpb; // boot sector info for this FS

    uint32_t fat_start_sector; // first FAT's starting sector index
    uint32_t fat_end_sector; //ending sector of the first FAT , honestly lets just use the first one only - that should be fine
    uint32_t first_data_sector; // first sector of data region (cluster #2)
    uint32_t total_clusters; // first sector of data region (cluster #2)

    uint32_t cwd_cluster; // cluster of current working directory

} FileSystem;

/* Mount/unmount functions */
bool fs_mount(FileSystem *fs, const char *image_path);
void fs_unmount(FileSystem *fs);

/* Part 1: print boot sector + computed filesystem information */
void cmd_info(const FileSystem *fs);

/* Create a new directory in the current working directory */
bool fs_mkdir(FileSystem *fs, const char *name);

/* Create a new empty file (size 0) in the current working directory */
bool fs_creat(FileSystem *fs, const char *name);

/* Part 2: Navigation commands */
/* List directory contents of the current working directory */
void fs_ls( FileSystem *fs);

/* Change current working directory to DIRNAME */
bool fs_cd(FileSystem *fs, const char *dirname);

/* Return the full path of the current working directory. The returned
 * string is dynamically allocated and must be freed by the caller. */
CurrentDirectory getcwd(FileSystem *fs);

size_t checkExists(char* filename, FileSystem* fs);

size_t checkIsFile(char* filename, FileSystem* fs);

uint32_t getStartCluster(char* filename, FileSystem* fs);

uint32_t getFileSize(char* filename, FileSystem* fs);

uint32_t readFile(uint32_t startOffset, uint32_t sizeToRead, char* filename, FileSystem* fs);

uint32_t writeToFile(const char* filename, const char* bytesToWrite, uint32_t startOffset, FileSystem* fs , OpenFile* file );

bool fs_rm(FileSystem *fs, char *filename, struct OpenFiles *open_files , char* cwd);

bool fs_rmdir(FileSystem *fs, const char *dirname, struct OpenFiles *open_files);

bool fs_mv(FileSystem *fs, char *src, char *dest, struct OpenFiles *open_files, CurrentDirectory *cwd_info);

void fs_ls_chain(const FileSystem *fs);