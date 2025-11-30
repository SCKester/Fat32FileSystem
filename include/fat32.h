#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/*
 * FAT32 Boot Sector (BIOS Parameter Block)
 * These fields are read directly from offsets inside the 512-byte boot sector.
 * Values are LITTLE-ENDIAN in the disk image, so they must be converted.
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
 * Maintains all state needed for mounted FAT32 operations:
 *  - parsed boot sector fields
 *  - FAT & data region offsets
 *  - a FILE* handle to the disk image
 *  - current working directory cluster (for future parts)
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

typedef struct { 
    char fileName[12]; // null terminated file name
    char permissions[3]; //can be any combination of rw , wr , r , w , is null terminated
    size_t index; //opened file index
    uint32_t offset; //offset of the file "pointer" inside the file , used to calculate the position of the global filsystem pointer - intialized to 0
    int open; //if file is open, if 0 then file is closed and we can disregard this entry
    uint32_t startCluster; //start cluster of file, we use this to diff between files with same name in different directories
} OpenFile;

struct OpenFiles {
    OpenFile files[10]; //all of the open files in the file system
};

struct OpenFiles getOpenFilesStruct() { //returnns intialized openfiles object

    struct OpenFiles files;

    for( int i = 0 ; i < 10 ; i++ ) {
        files.files[i].index = i;
        files.files[i].offset = 0; 
        files.files->open = 0;
    }
}

//scans for first available index and creates open file entry on that index , returns the index , -1 if error
//offset set to 0
size_t openFile( struct OpenFiles* files ,  char* fileName , char* permisssions , uint32_t startCluster ) {
    
    size_t index = -1;

    for ( int i = 0 ; i < 10 ; i++ ) {
        if( files->files[i].open == 0 ) {
            index = i;
            break;
        }
    }

    if(index == -1) {
        return index;
    }

    strcpy( files->files[index].fileName , fileName );
    strcpy( files->files[index].permissions , permisssions );

    files->files[index].offset = 0;
    files->files[index].open = 1;
    files->files[index].startCluster = startCluster;

    return index;
}

//closes file with starting cluster of StartCluster, returns index of closed file or -1 if not found
size_t closeFile( struct OpenFiles* files , uint32_t startCluster ) {

    size_t index = -1;

    for ( int i = 0 ; i < 10 ; i++ ) {
        if( files->files[i].startCluster == startCluster ) {
            index = i;
            break;
        }
    }

    if( index == -1 ) {
        return index;
    }

    files->files[index].open = 0; //set open flag to false so can be overwritten

    return index;
}

/* Mount/unmount functions */
bool fs_mount(FileSystem *fs, const char *image_path);
void fs_unmount(FileSystem *fs);

/* Part 1: print boot sector + computed filesystem information */
void cmd_info(const FileSystem *fs);

/* Create a new directory in the current working directory */
bool fs_mkdir(FileSystem *fs, const char *name);

/* Create a new empty file (size 0) in the current working directory */
bool fs_creat(FileSystem *fs, const char *name);