#define _POSIX_C_SOURCE 200809L
#include "fat32.h"
#include "fat32_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/*
 * fat32_navigation.c
 * Part 2: Navigation Commands (cd, ls, getcwd)
 */

/* =============================================================================
 * ls command
 * ============================================================================= */

/* Lists all directory entries in the current working directory.
 * Prints the name field for each directory and file, including "." and "..".
 * Skips deleted entries (0xE5) and long filename entries (0x0F).
 */
void fs_ls(const FileSystem *fs) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)malloc(cluster_size);
    if (!buf) {
        printf("Error: memory allocation failed\n");
        return;
    }

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0) {
        printf("Error: failed to seek to directory cluster\n");
        free(buf);
        return;
    }

    if (fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        printf("Error: failed to read directory cluster\n");
        free(buf);
        return;
    }

    /* Iterate through all 32-byte directory entries */
    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        /* 0x00 means this and all following entries are free */
        if (entry[0] == 0x00) {
            break;
        }

        /* 0xE5 means deleted, skip */
        if (entry[0] == 0xE5) {
            continue;
        }

        /* Skip long filename entries (attribute 0x0F) */
        unsigned char attr = entry[11];
        if ((attr & 0x0F) == 0x0F) {
            continue;
        }

        /* Extract and print the 11-byte short name */
        char name[12];
        memcpy(name, entry, 11);
        name[11] = '\0';

        /* Print the name (it will have trailing spaces, but that's okay) */
        printf("%s\n", name);
    }

    free(buf);
}

/* =============================================================================
 * cd command
 * ============================================================================= */

/* Changes the current working directory to DIRNAME.
 * Returns true on success, false on failure.
 * Prints an error message if DIRNAME does not exist or is not a directory.
 */
bool fs_cd(FileSystem *fs, const char *dirname) {
    if (!dirname || dirname[0] == '\0') {
        printf("Error: cd requires a directory name\n");
        return false;
    }

    /* Build FAT short name */
    char short_name[11];
    build_short_name(short_name, dirname);

    /* Read directory entries to find the target */
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

    unsigned char *buf = (unsigned char *)malloc(cluster_size);
    if (!buf) {
        printf("Error: memory allocation failed\n");
        return false;
    }

    long dir_offset = cluster_to_offset(fs, fs->cwd_cluster);
    if (fseek(fs->image, dir_offset, SEEK_SET) != 0) {
        printf("Error: failed to seek to directory cluster\n");
        free(buf);
        return false;
    }

    if (fread(buf, 1, cluster_size, fs->image) != cluster_size) {
        printf("Error: failed to read directory cluster\n");
        free(buf);
        return false;
    }

    /* Search for the directory entry */
    bool found = false;
    uint32_t target_cluster = 0;

    for (uint32_t off = 0; off < cluster_size; off += 32) {
        unsigned char *entry = buf + off;

        /* 0x00 means this and all following entries are free */
        if (entry[0] == 0x00) {
            break;
        }

        /* 0xE5 means deleted, skip */
        if (entry[0] == 0xE5) {
            continue;
        }

        /* Skip long filename entries (attribute 0x0F) */
        unsigned char attr = entry[11];
        if ((attr & 0x0F) == 0x0F) {
            continue;
        }

        /* Compare short name */
        if (memcmp(entry, short_name, 11) == 0) {
            /* Check if it's a directory (attribute 0x10) */
            if ((attr & 0x10) == 0) {
                printf("Error: '%s' is not a directory\n", dirname);
                free(buf);
                return false;
            }

            /* Extract the cluster number from the entry */
            /* Cluster is stored as: high word at bytes 20-21, low word at bytes 26-27 (little-endian) */
            target_cluster = ((uint32_t)entry[21] << 24) | ((uint32_t)entry[20] << 16) |
                           ((uint32_t)entry[27] << 8) | (uint32_t)entry[26];

            found = true;
            break;
        }
    }

    free(buf);

    if (!found) {
        printf("Error: '%s' does not exist\n", dirname);
        return false;
    }

    /* In FAT32, if target cluster is 0, it means the root directory */
    if (target_cluster == 0) {
        target_cluster = fs->bpb.root_cluster;
    }

    /* Update current working directory */
    fs->cwd_cluster = target_cluster;
    return true;
}

/* =============================================================================
 * getcwd helper
 * ============================================================================= */

/* getcwd()
 * Builds a full path from the current working directory by walking up
 * the directory tree following the ".." entries until the root cluster.
 * Returns a dynamically allocated c string and its size (caller must free) containing the
 * path in the form "/dir1/dir2". root returns "/".
*/
CurrentDirectory getcwd( FileSystem *fs ) {
    const Fat32BootSector *bpb = &fs->bpb;
    uint32_t root = bpb->root_cluster;
    uint32_t cur = fs->cwd_cluster;
    CurrentDirectory directory;

    directory.size = -1; //err

    /* If at root, return "/" */
    if (cur == root) {
        char* s = (char*) malloc(2);

        if (s) { s[0] = '/'; s[1] = '\0'; }

        directory.size = 2;

        directory.cwd = s;

        return directory;
    }

    /* Collect path components (child -> parent). We'll store pointers in a
     * dynamic array and later join them in reverse order. */
    size_t cap = 8;
    size_t nseg = 0;
    char** segments = (char**) malloc(cap * sizeof(char*));

    while (cur != root) {
        // Read current directory cluster to find ".." entry (parent)

        const uint32_t cluster = cur;

        uint32_t cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;

        unsigned char* buf = (unsigned char*) malloc(cluster_size);

        if (!buf) break;

        long dir_offset = cluster_to_offset(fs, cluster);
        if (fseek(fs->image, dir_offset, SEEK_SET) != 0 ||
            fread(buf, 1, cluster_size, fs->image) != cluster_size) {
            free(buf);
            break;
        }

        uint32_t parent = 0;
        // Find the entry for ".."
        for (uint32_t off = 0; off < cluster_size; off += 32) {

            unsigned char *entry = buf + off;

            if (entry[0] == 0x00) break;

            if (entry[0] == 0xE5) continue;

            unsigned char attr = entry[11];

            if ((attr & 0x0F) == 0x0F) continue; // long name

            if (entry[0] == '.' && entry[1] == '.') {
                parent = ((uint32_t)entry[21] << 24) | ((uint32_t)entry[20] << 16) |
                         ((uint32_t)entry[27] << 8) | (uint32_t)entry[26];
                break;
            }
        }

        free(buf);

        if (parent == 0) {
            parent = root;
        }


        cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
        buf = (unsigned char*)malloc(cluster_size);
        if (!buf) break;

        dir_offset = cluster_to_offset(fs, parent);
        if (fseek(fs->image, dir_offset, SEEK_SET) != 0 ||
            fread(buf, 1, cluster_size, fs->image) != cluster_size) {
            free(buf);
            break;
        }

        char found_name[13];
        bool found = false;

        for (uint32_t off = 0; off < cluster_size; off += 32) {
            unsigned char *entry = buf + off;

            if (entry[0] == 0x00) break;

            if (entry[0] == 0xE5) continue;

            unsigned char attr = entry[11];

            if ((attr & 0x0F) == 0x0F) continue; // long name

            // Extract cluster of this entry
            uint32_t ent_cluster =
                ((uint32_t)entry[21] << 24) |
                ((uint32_t)entry[20] << 16) |
                ((uint32_t)entry[27] <<  8) |
                (uint32_t)entry[26];

            if (ent_cluster == cur) {

                char name_part[9];
                char ext_part[4];
                memcpy(name_part, entry, 8);
                name_part[8] = '\0';
                memcpy(ext_part, entry + 8, 3);
                ext_part[3] = '\0';

                for (int i = 7; i >= 0; --i) {
                    if (name_part[i] == ' ') name_part[i] = '\0'; else break;
                }
                for (int i = 2; i >= 0; --i) {
                    if (ext_part[i] == ' ') ext_part[i] = '\0'; else break;
                }

                if (ext_part[0] != '\0') {
                    snprintf(found_name, sizeof(found_name), "%s.%s", name_part, ext_part);
                } else {
                    snprintf(found_name, sizeof(found_name), "%s", name_part);
                }

                found = true;
                break;
            }
        }

        free(buf);

        if (!found) break;


        if (nseg + 1 > cap) {
            size_t ncap = cap * 2;
            char** tmp = (char**) realloc(segments, ncap * sizeof(char*));
            if (!tmp) break;
            segments = tmp;
            cap = ncap;
        }

        segments[nseg++] = strdup(found_name);


        cur = parent;
    }


    // / as fallback
    if (nseg == 0) {
        free(segments);
        char* s = (char*) malloc(2);
        if (s) { s[0] = '/'; s[1] = '\0'; }

        directory.size = 2;
        directory.cwd = s;

        return directory;
    }

    //compute total length for the joined path: leading '/', segments and '/' between
    size_t total = 1;

    for (size_t i = 0; i < nseg; ++i)
        total += strlen(segments[i]) + 1;


    char* path = (char*) malloc(total + 1);

    if (!path) {

        for (size_t i = 0; i < nseg; i++)
            free(segments[i]);

        free(segments);

        return directory;
    }

    char* p = path;
    *p++ = '/';

    //segments are from child->parent; we need to print in reverse
    for (size_t i = 0; i < nseg; i++) {

        char* seg = segments[nseg - 1 - i];

        size_t len = strlen(seg);

        memcpy(p, seg, len);

        p += len;

        if (i + 1 < nseg) *p++ = '/';

        free(seg);
    }

    *p = '\0';

    free(segments);

    directory.size = total;
    directory.cwd = path;

    return directory;
}
