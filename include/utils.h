#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "lexer.h"

typedef struct { 
    char fileName[12]; // null terminated file name
    char* filePath; //dynamically allocated MUST BE FREED
    int permissions; //1 is read , 2 is write ,  3 is read/write - -1 is none or not valid
    size_t index; //opened file index
    uint32_t offset; //offset of the file "pointer" inside the file , used to calculate the position of the global filsystem pointer - intialized to 0
    int open; //if file is open, if 0 then file is closed and we can disregard this entry
    uint32_t startCluster; //start cluster of file, we use this to diff between files with same name in different directories
} OpenFile;

struct OpenFiles {
    OpenFile files[10]; //all of the open files in the file system
};

struct OpenFiles getOpenFilesStruct();

typedef struct {
    size_t size; //size of cwd arr
    char* cwd; //cwd is expected to be a dynamically allocated array freed by creator
} CurrentDirectory;

//scans for first available index and creates open file entry on that index , returns the index , -1 if error
//offset set to 0
int openFile( struct OpenFiles* files ,  char* fileName , int mode , uint32_t startCluster , CurrentDirectory* direc );

//closes file with starting cluster of StartCluster, returns index of closed file or -1 if not found
int closeFile( struct OpenFiles* files , uint32_t startCluster , CurrentDirectory* direc , char* filename );

void closeAllFiles( struct OpenFiles* files );

//1 is read , 2 is write ,  3 is read/write - 0 is none or not valid
size_t getReadWrite( tokenlist* tokens  );

int checkIsOpen( struct OpenFiles* files , char* cwd , char* filename );

void printOpenFiles( struct OpenFiles* files );

int writeFileOffset( struct OpenFiles* files , uint32_t startCluster, char* filename, char* cwd , uint32_t newOffset );

OpenFile* getOpenFile( struct OpenFiles* files , uint32_t startCluster , CurrentDirectory* direc , char* filename  );