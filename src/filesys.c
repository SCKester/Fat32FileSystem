#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "fat32.h"

/*
 * Main interactive shell for FAT32 project.
 *
 * Supported commands (Part 1 + Part 3):
 *   info        → print filesystem metadata
 *   mkdir NAME  → create directory in cwd
 *   creat NAME  → create empty file in cwd
 *   exit        → quit the shell
 *
 * The shell repeatedly:
 *   1. Prints a prompt: "<image>/>"
 *   2. Reads a line using get_input()
 *   3. Tokenizes via get_tokens()
 *   4. Dispatches to the correct handler
 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <fat32_image>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FileSystem fs;
    if (!fs_mount(&fs, argv[1])) {
        return EXIT_FAILURE;
    }

    const char *cwd = "/";  // Part 1 doesn’t require real paths yet

    while (1) {
        printf("%s%s> ", fs.image_name, cwd);
        fflush(stdout);

        char *input = get_input();
        if (!input) break;

        tokenlist *tokens = get_tokens(input);

        if (tokens->size > 0) {
            char *cmd = tokens->items[0];

            if (strcmp(cmd, "info") == 0) {
                /*
                * info
                * Prints filesystem metadata (boot sector fields + computed values).
                */
                cmd_info(&fs);
            }
              else if (strcmp(cmd, "mkdir") == 0) {
                /*
                * mkdir [DIRNAME]
                * DIRNAME must be a short FAT 8.3 style name (1–11 chars).
                * fs_mkdir() handles cluster allocation and directory entry creation.
                */
                if (tokens->size != 2) {
                    printf("Error: usage: mkdir [DIRNAME]\n");
                } else {
                    if (!fs_mkdir(&fs, tokens->items[1])) {
                        /* fs_mkdir prints its own error message */
                    }
                }
            }
            else if (strcmp(cmd, "creat") == 0) {
                 /*
                * creat [FILENAME]
                * Creates an empty file (size=0).
                * No data cluster is allocated yet.
                */
                if (tokens->size != 2) {
                    printf("Error: usage: creat [FILENAME]\n");
                } else {
                    if (!fs_creat(&fs, tokens->items[1])) {
                        /* fs_creat prints its own error message */
                    }
                }
            }
            else if (strcmp(cmd, "exit") == 0) {
                /*
                * exit
                * Cleanly quit the shell:
                *  - free tokens
                *  - free input buffer
                *  - leave loop and unmount
                */
                free_tokens(tokens);
                free(input);
                break;
            }
            else {
                printf("Error: unknown command '%s'\n", cmd);
            }
        }

        free_tokens(tokens);
        free(input);
    }

    fs_unmount(&fs);
    return EXIT_SUCCESS;
}

