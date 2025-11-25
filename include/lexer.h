#pragma once

#include <stdlib.h>
#include <stdbool.h>

/*
 * tokenlist:
 * A dynamically-sized array of strings returned by get_tokens().
 * items[i] is a null-terminated C string for each token.
 * items[size] is always NULL to mark end.
 */
typedef struct {
    char ** items;
    size_t size;
} tokenlist;

/*
 * get_input()
 * Reads an entire user input line of ANY length (dynamically reallocates).
 * Returns a malloc'd string which caller must free.
 */
char * get_input(void);

/*
 * get_tokens()
 * Splits an input string on spaces into a tokenlist.
 */
tokenlist * get_tokens(char *input);
/* Constructors / destructors for tokenlist */
tokenlist * new_tokenlist(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);
