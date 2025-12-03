#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "lexer.h"
#include "fat32.h"

typedef struct { 
    char fileName[12]; // null terminated file name
    char* filePath; //dynamically allocated MUST BE FREED
    size_t permissions; //1 is read , 2 is write ,  3 is read/write - -1 is none or not valid
    size_t index; //opened file index
    uint32_t offset; //offset of the file "pointer" inside the file , used to calculate the position of the global filsystem pointer - intialized to 0
    int open; //if file is open, if 0 then file is closed and we can disregard this entry
    uint32_t startCluster; //start cluster of file, we use this to diff between files with same name in different directories
} OpenFile;

struct OpenFiles {
    OpenFile files[10]; //all of the open files in the file system
};

struct OpenFiles getOpenFilesStruct();

//scans for first available index and creates open file entry on that index , returns the index , -1 if error
//offset set to 0
size_t openFile( struct OpenFiles* files ,  char* fileName , size_t mode , uint32_t startCluster , CurrentDirectory direc );

//closes file with starting cluster of StartCluster, returns index of closed file or -1 if not found
size_t closeFile( struct OpenFiles* files , uint32_t startCluster );

void closeAllFiles( struct OpenFiles* files );

//1 is read , 2 is write ,  3 is read/write - -1 is none or not valid
size_t getReadWrite( tokenlist* tokens  );

size_t checkIsOpen( uint32_t startCluster , struct OpenFiles* files );

void printOpenFiles( struct OpenFiles* files );

size_t writeFileOffset( struct OpenFiles* files , uint32_t startCluster , uint32_t newOffset );

OpenFile* getOpenFile( struct OpenFiles* files , uint32_t startCluster );