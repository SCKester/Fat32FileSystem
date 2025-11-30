#include "utils.h"


struct OpenFiles getOpenFilesStruct() { //returnns intialized openfiles object

    struct OpenFiles files;

    for( int i = 0 ; i < 10 ; i++ ) {
        files.files[i].index = i;
        files.files[i].offset = 0; 
        files.files->open = 0;
    }

    return files;
}

size_t openFile( struct OpenFiles* files ,  char* fileName , char* permisssions , uint32_t startCluster ) {
    
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

    files->files[index].open = 0; //set open flag to false so can be overwritten

    return index;
}

