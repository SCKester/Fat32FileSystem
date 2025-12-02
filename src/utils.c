#include "utils.h"
#include <stdio.h>

//-1 if open , 0 if not open
static size_t checkIsOpen( uint32_t startCluster , struct OpenFiles* files ) {

    printf("%i" , startCluster);

    for ( size_t i = 0 ; i < 10 ; i++ ) {
        if( files->files[i].startCluster == startCluster ) {
            return -1;
        }
    }

    return 0;
}

struct OpenFiles getOpenFilesStruct() { //returnns intialized openfiles object

    struct OpenFiles files;

    for( int i = 0 ; i < 10 ; i++ ) {
        files.files[i].index = i;
        files.files[i].offset = 0; 
        files.files[i].open = 0;
        files.files[i].startCluster = -1;
    }

    return files;
}

//we can do a bit of cheating here because we know files will only be opened in the cwd
//and that all files will only be scanned in the cwd
// -1 if failed , 0 if succeeded
size_t openFile( struct OpenFiles* files ,  char* fileName , size_t mode , uint32_t startCluster , CurrentDirectory direc ) {
    
    size_t index = -1;

    if( checkIsOpen( startCluster , files ) == -1 ) {
        return -1;
    }

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
    files->files->permissions = mode;

    files->files[index].offset = 0;
    files->files[index].open = 1;
    files->files[index].startCluster = startCluster;

    char* path = (char*) malloc( sizeof(char) * direc.size + 1 );

    strcpy( path , direc.cwd );

    files->files[index].filePath = path;

    return index;
}

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

    free( files->files[index].filePath );

    files->files[index].open = 0; //set open flag to false so can be overwritten

    return index;
}

//call this on exit to close all files ( free mem )
void closeAllFiles( struct OpenFiles* files ) {

    for ( int i = 0 ; i < 10 ; i++ ) {
        if( files->files[i].open == 1 ) {
            printf("index: %i" , i);
            free( files->files[i].filePath );
        }
    }

}

size_t getReadWrite( tokenlist* tokens  ) { 

    if( strcmp( tokens->items[2] , "-r" ) != 0 && 
    strcmp( tokens->items[2] , "-w" ) != 0 && 
    strcmp( tokens->items[2] , "-rw" ) != 0 && 
    strcmp( tokens->items[2] , "-wr" ) != 0 ) {
        return -1;
    }

    if( strcmp( tokens->items[2] , "-rw" ) == 0 || 
    strcmp( tokens->items[2] , "-wr" ) == 0  ) {
        return 3;
    }

    if( strcmp( tokens->items[2] , "-w" ) == 0 ) {
        return 2;
    }

    if( strcmp( tokens->items[2] , "-r" ) == 0 ) {
        return 2;
    }


    return -1;
}

