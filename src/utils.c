#include "utils.h"
#include <stdio.h>


struct OpenFiles getOpenFilesStruct() { //returnns intialized openfiles object

    struct OpenFiles files;

    for( int i = 0 ; i < 10 ; i++ ) {
        files.files[i].index = i;
        files.files[i].offset = 0; 
        files.files[i].open = 0;
    }

    return files;
}

//we can do a bit of cheating here because we know files will only be opened in the cwd
//and that all files will only be scanned in the cwd
size_t openFile( struct OpenFiles* files ,  char* fileName , char* permisssions , uint32_t startCluster , char* path ) {
    
    size_t index = -1;

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
    strcpy( files->files[index].permissions , permisssions );

    files->files[index].offset = 0;
    files->files[index].open = 1;
    files->files[index].startCluster = startCluster;

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