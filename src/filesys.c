#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "fat32.h"

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
                cmd_info(&fs);
            }
            else if (strcmp(cmd, "exit") == 0) {
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

