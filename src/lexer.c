#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *get_input(void) {
    char *buffer = NULL;
    int bufsize = 0;
    char line[5];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        int addby = 0;
        char *newln = strchr(line, '\n');

        if (newln != NULL)
            addby = (int)(newln - line);
        else
            addby = (int)sizeof(line) - 1;

        buffer = (char *)realloc(buffer, bufsize + addby);
        memcpy(&buffer[bufsize], line, addby);
        bufsize += addby;

        if (newln != NULL)
            break;
    }

    buffer = (char *)realloc(buffer, bufsize + 1);
    buffer[bufsize] = '\0';
    return buffer;
}

tokenlist *new_tokenlist(void) {
    tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
    tokens->size = 0;
    tokens->items = (char **)malloc(sizeof(char *));
    tokens->items[0] = NULL; /* NULL-terminated */
    return tokens;
}

void add_token(tokenlist *tokens, char *item) {
    int i = (int)tokens->size;

    tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
    tokens->items[i] = (char *)malloc(strlen(item) + 1);
    tokens->items[i + 1] = NULL;

    strcpy(tokens->items[i], item);
    tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
    char *buf = (char *)malloc(strlen(input) + 1);
    strcpy(buf, input);

    tokenlist *tokens = new_tokenlist();
    char *tok = strtok(buf, " ");

    while (tok != NULL) {
        add_token(tokens, tok);
        tok = strtok(NULL, " ");
    }

    free(buf);
    return tokens;
}

void free_tokens(tokenlist *tokens) {
    for (int i = 0; i < (int)tokens->size; i++) {
        free(tokens->items[i]);
    }
    free(tokens->items);
    free(tokens);
}


