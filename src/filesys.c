#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "fat32.h"
#include "utils.h"

/*
 * Main interactive shell for FAT32 project.
 *
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

    struct OpenFiles openFiles = getOpenFilesStruct(); //holds all the open files (up to 10)

    CurrentDirectory cwd;  // Part 1 doesn’t require real paths yet

    while (1) {

        cwd = getcwd( &fs );

        printf("%s%s> ", fs.image_name, cwd.cwd);
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
            else if (strcmp(cmd, "ls") == 0) {
                /*
                * ls
                * Lists all directory entries in the current working directory.
                * Prints the name field for each entry including "." and "..".
                */
                fs_ls(&fs);
            }
            else if (strcmp(cmd, "cd") == 0) {
                /*
                * cd [DIRNAME]
                * Changes the current working directory to DIRNAME.
                * Prints an error if DIRNAME does not exist or is not a directory.
                */
                if (tokens->size != 2) {
                    printf("Error: usage: cd [DIRNAME]\n");
                } else {
                    if (!fs_cd(&fs, tokens->items[1])) {
                        /* fs_cd prints its own error message */
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
                free(cwd.cwd);
                break;
            }
            else if ( strcmp( cmd , "open" ) == 0 ) {
                
                if( tokens->size != 3 || getReadWrite( tokens ) == 0 ) {
                    printf("Error: usage: open [FILENAME] [FLAGS]\n");
                }
                else {

                    if( checkExists( tokens->items[1] , &fs ) == -1  || checkIsFile( tokens->items[1] , &fs ) == -1 ) { //file/directory doesnt exist
                        printf("Error: file does not exist\n" );
                    }
                    else {

                        //we now know that filename is a file in cwd

                        CurrentDirectory direc = getcwd( &fs );

                        if( openFile( &openFiles , tokens->items[1] , getReadWrite( tokens ) , getStartCluster( tokens->items[1] , &fs ) , &direc ) == -1 ) {
                            printf("Error: cannot open file, likely already open.\n");
                        }
                        free( direc.cwd );
                    }

                }
            }
            else if ( strcmp( cmd , "close") == 0 ) {

                if( tokens->size != 2 ) {
                    printf("Error: usage close [FILENAME]\n");
                }
                else {

                    if( checkExists( tokens->items[1] , &fs ) == -1 || checkIsFile( tokens->items[1] , &fs ) == -1 ) {
                        printf("Error: file does not exist - maybe it is a directory?\n");
                    }
                    else {
                        //file exists and is a fikle indeed check if open?

                        uint32_t startCluster = getStartCluster( tokens->items[1] , &fs );

                        if( checkIsOpen( startCluster , &openFiles , cwd.cwd , tokens->items[1] ) == 0 ) { //file not open , error
                            printf("Error: file is not open.\n");
                        }
                        else {
                            //file is open and a file, we can close it
                            if( closeFile( &openFiles , startCluster , &cwd , tokens->items[1] ) == -1)
                                printf("Error: cannot close file...\n");
                        }
                    }
                }
            }
            else if ( strcmp( cmd , "lsof" ) == 0 ) {

                if( tokens->size != 1 ) {
                    printf("Error: usage, lsof...\n");
                }
                else {

                    printOpenFiles( &openFiles );
                }
            }
            else if ( strcmp( cmd , "lseek") == 0 ) {

                if( tokens->size != 3 ) {
                    printf("Error: usage - lseek [FILENAME] [OFFSET]\n");
                }
                else {

                    if( checkIsFile( tokens->items[1] , &fs ) == -1 ) {
                        printf("Error: file does not exist.");
                    }
                    else {

                        char* endptr = NULL;
                        
                        uint32_t newOffset = strtoull( tokens->items[2] , &endptr , 10);

                        if ( strcmp( endptr , "\0" ) != 0 ) {
                            printf("Error: invalid offset number: %s , offset is not numeric\n" , tokens->items[2] );
                        }
                        else {

                            if( checkIsOpen( getStartCluster( tokens->items[1] , &fs ) , &openFiles , cwd.cwd , tokens->items[1] ) == 0 ) { //file not open, error
                                printf("Error: file, %s is not open in cwd\n" , tokens->items[2] );
                            }
                            else {

                                //file is now understood to be open and in cwd, offset is also valid assumed
                                //now check if offset larger than file
                                if( newOffset > getFileSize( tokens->items[1] , &fs ) ) {
                                    printf("Error: offset %s larger than file size %u\n" , tokens->items[1] , getFileSize( tokens->items[2] , &fs ) );
                                }
                                else {
                                    //we can now write offset to oopen file
                                    if( writeFileOffset( &openFiles , getStartCluster( tokens->items[1] , &fs ) , newOffset ) == -1 ) {
                                        printf("Error: unable to write offset to file.\n");
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if ( strcmp( cmd , "read" ) == 0 ) {

                if( tokens->size != 3 ) {
                    printf("Error: Usage , read [filename] [size]\n");
                    goto skip;
                }

                char* endptr = NULL;
                
                uint32_t bytesToRead = strtoull( tokens->items[2] , &endptr , 10);

                if( tokens->size != 3  || strcmp( endptr , "\0") != 0 ) {
                    printf("Error: Usage - read [FILENAME] [SIZE]\n");
                }
                else {

                    if( checkIsFile( tokens->items[1] , &fs ) == -1 ) {
                        printf("Error: file does not exist...\n");
                    }
                    else {

                        if( checkIsOpen( getStartCluster( tokens->items[1] , &fs ) , &openFiles , cwd.cwd , tokens->items[1] ) == 0 ) {
                            printf("Error: file is not open...\n");
                        }
                        else {
                            //file is assumed open and exdsiting in cwd
                            //we now read from file and update offset

                            //now check open to read

                            OpenFile* file = getOpenFile( &openFiles , 
                                getStartCluster( tokens->items[1] , &fs ) , &cwd , tokens->items[1] );

                            if( file == NULL || ( file->permissions != 1 && file->permissions != 3 ) ) {

                                if( file == NULL) {
                                    printf("fick\n");
                                }
                                //file not open somehow or file not oopened with read
                                printf("Error: file not opened in read mode.\n");
                            }
                            else {
                                uint32_t bytesRead = readFile( file->offset , bytesToRead , tokens->items[1] , &fs );

                                file->offset += bytesRead;
                            }
                        }
                    }
                }
                
            }
            else if (strcmp(cmd, "rm") == 0) {

                if (tokens->size != 2) {
                    printf("Error: usage: rm [FILENAME]\n");
                } else {
                    if (!fs_rm(&fs, tokens->items[1], &openFiles , cwd.cwd )) {
                        printf("Error: error removing file.\n");
                    }
                }
            }
            else if (strcmp(cmd, "rmdir") == 0) {

                if (tokens->size != 2) {
                    printf("Error: usage: rmdir [DIRNAME]\n");
                } else {
                    if (!fs_rmdir(&fs, tokens->items[1], &openFiles)) {
                        printf("Error: error removing directory.\n");
                    }
                }
            }
            else if ( strcmp( cmd , "write" ) == 0 ) {
                
                if( tokens->size != 3) {
                    printf("error: %lu" , tokens->size);
                    printf("Error: Usage , write [FILENAME] {STRING}\n");
                }
                else {

                    if( checkIsFile( tokens->items[1] , &fs ) == -1 ) {
                        printf("Error: file not found...\n");
                    }
                    else {

                        if( checkIsOpen( getStartCluster( tokens->items[1] , &fs ) , &openFiles , cwd.cwd , tokens->items[1] ) == 0 ) {
                            printf("Error: file is not open..");
                        }
                        else {
                            
                            OpenFile* file = getOpenFile( &openFiles , getStartCluster( tokens->items[1] , &fs ) , &cwd , tokens->items[1] );

                            if( file == NULL ) {
                                printf("Error: cannot get open file.\n");
                            }
                            else {

                                if( file->permissions == 1 ) { //read only permissions
                                    printf("Error: file opened in read only mode.\n");
                                }
                                else {
                                    //file now assumed to be open and valid
                                    uint32_t bytesWritten = writeToFile( tokens->items[1] , tokens->items[2] , file->offset ,  &fs , file ); 

                                    if ( bytesWritten == 0 ) {
                                        printf("No Bytes Written...\n");
                                    }
                                    else {
                                        file->offset += bytesWritten;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else {
                printf("Error: unknown command '%s'\n", cmd);
            }
        }

        skip:

        free_tokens(tokens);
        free(input);
        free(cwd.cwd);
    }

    fs_unmount(&fs);
    closeAllFiles( &openFiles );
    return EXIT_SUCCESS;
}

