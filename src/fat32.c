#include "fat32.h"
#include <string.h>
#include <stdio.h>

/* Little-endian readers */
static uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/* Mount FAT32 filesystem */
bool fs_mount(FileSystem *fs, const char *image_path) {
    memset(fs, 0, sizeof(*fs));

    fs->image = fopen(image_path, "rb");
    if (!fs->image) {
        fprintf(stderr, "Error: cannot open image file '%s'\n", image_path);
        return false;
    }

    strncpy(fs->image_name, image_path, sizeof(fs->image_name)-1);

    unsigned char boot[512];
    if (fread(boot, 1, sizeof(boot), fs->image) != 512) {
        fprintf(stderr, "Error: cannot read boot sector\n");
        fclose(fs->image);
        fs->image = NULL;
        return false;
    }

    Fat32BootSector *bpb = &fs->bpb;

    bpb->bytes_per_sector      = read_le16(&boot[0x0B]);
    bpb->sectors_per_cluster   = boot[0x0D];
    bpb->reserved_sector_count = read_le16(&boot[0x0E]);
    bpb->num_fats              = boot[0x10];

    uint16_t total16 = read_le16(&boot[0x13]);
    uint32_t total32 = read_le32(&boot[0x20]);
    bpb->total_sectors = (total16 != 0) ? total16 : total32;

    bpb->fat_size_sectors = read_le32(&boot[0x24]);
    bpb->root_cluster     = read_le32(&boot[0x2C]);

    return true;
}

/* Unmount filesystem */
void fs_unmount(FileSystem *fs) {
    if (fs->image) {
        fclose(fs->image);
        fs->image = NULL;
    }
}

/* info command */
void cmd_info(const FileSystem *fs) {
    const Fat32BootSector *bpb = &fs->bpb;

    uint32_t bytes_per_sector    = bpb->bytes_per_sector;
    uint32_t sectors_per_cluster = bpb->sectors_per_cluster;
    uint32_t reserved            = bpb->reserved_sector_count;
    uint32_t num_fats            = bpb->num_fats;
    uint32_t fat_size            = bpb->fat_size_sectors;
    uint32_t total_sectors       = bpb->total_sectors;
    uint32_t root_cluster        = bpb->root_cluster;

    uint32_t data_sectors = total_sectors - (reserved + num_fats * fat_size);
    uint32_t total_clusters = data_sectors / sectors_per_cluster;

    uint32_t entries_per_fat = (fat_size * bytes_per_sector) / 4;

    unsigned long long image_bytes =
        (unsigned long long)total_sectors * (unsigned long long)bytes_per_sector;

    printf("position of root cluster (in cluster #): %u\n", root_cluster);
    printf("bytes per sector: %u\n", bytes_per_sector);
    printf("sectors per cluster: %u\n", sectors_per_cluster);
    printf("total # of clusters in data region: %u\n", total_clusters);
    printf("# of entries in one FAT: %u\n", entries_per_fat);
    printf("size of image (in bytes): %llu\n", image_bytes);
}

