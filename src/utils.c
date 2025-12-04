#include "utils.h"
#include <stdio.h>

//-1 if open , 0 if not open
int checkIsOpen( uint32_t startCluster , struct OpenFiles* files , char* cwd , char* filename ) {

    for ( size_t i = 0 ; i < 10 ; i++ ) {

        OpenFile* file = &(files->files[i]);

        if( file->open == 1 && file->startCluster == startCluster && strcmp( file->fileName , filename) == 0 && strcmp( file->filePath , cwd ) == 0 ) {
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
        files.files[i].startCluster = 0;
        files.files[i].permissions = -1;

    }

    return files;
}

//we can do a bit of cheating here because we know files will only be opened in the cwd
//and that all files will only be scanned in the cwd
// -1 if failed , 0 if succeeded
int openFile( struct OpenFiles* files ,  char* fileName , int mode , uint32_t startCluster , CurrentDirectory* direc ) {
    
    size_t index = -1;

    if( checkIsOpen( startCluster , files , direc->cwd , fileName ) == -1 ) {
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

    files->files[index].permissions = mode;

    files->files[index].offset = 0;
    files->files[index].open = 1;
    files->files[index].startCluster = startCluster;

    char* path = (char*) malloc( sizeof(char) * direc->size + 1 );

    strcpy( path , direc->cwd );

    files->files[index].filePath = path;

    return index;
}

//returns -1 on error, otherwise index of closed file
int closeFile( struct OpenFiles* files , uint32_t startCluster , CurrentDirectory* direc , char* filename ) {

    if( checkIsOpen( startCluster , files , direc->cwd , filename ) == 0 ) {
        return -1;
    }

    size_t index = -1;

    for ( int i = 0 ; i < 10 ; i++ ) {

        OpenFile* file = &(files->files[i]);

        if( file->open == 1 && 
            file->startCluster == startCluster && 
            strcmp( file->fileName , filename) == 0 && 
            strcmp( file->filePath , direc->cwd ) == 0)  {

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
            free( files->files[i].filePath );
        }
    }

}

size_t getReadWrite( tokenlist* tokens  ) { 

    if( strcmp( tokens->items[2] , "-r" ) != 0 && 
    strcmp( tokens->items[2] , "-w" ) != 0 && 
    strcmp( tokens->items[2] , "-rw" ) != 0 && 
    strcmp( tokens->items[2] , "-wr" ) != 0 ) {
        return 0;
    }

    if( strcmp( tokens->items[2] , "-rw" ) == 0 || 
    strcmp( tokens->items[2] , "-wr" ) == 0  ) {
        return 3;
    }

    if( strcmp( tokens->items[2] , "-w" ) == 0 ) {
        return 2;
    }

    if( strcmp( tokens->items[2] , "-r" ) == 0 ) {
        return 1;
    }


    return 0;
}


//TODO: fix jump opn unintialized balues issue with valgrind
void printOpenFiles( struct OpenFiles* files ) {

    size_t count = 0; 

    for ( size_t i = 0 ; i < 10 ; i++ ) {

        if( files->files[i].open == 1 ) {
            count++;
        }
    }

    if( count == 0 ){
        printf("No open files...\n");
        return;
    } 

    printf("INDEX\tNAME\tMODE\tOFFSET\tPATH\n");

    for ( size_t i = 0 ; i < 10 ; i++ ) {

        if( files->files[i].open == 0 ) { //not open
            continue;
        }

        int permission = files->files[i].permissions;

        if( files->files[i].open == 1 ) {
            printf("%lu\t%s\t%s\t%u\t%s%s%s%s\n" , files->files[i].index , 
                files->files[i].fileName , 
                permission == 1 ? "r" : permission == 2 ? "w" : "rw", 
                files->files[i].offset , 
                files->files[i].filePath , 
                strcmp(files->files[i].filePath , "/") == 0 ? "" : "/" ,
                files->files[i].fileName , 
                "/"
            );
        }
    }

}

//writes file offset and returns 0 on success writing , -1 otherwise if fail
int writeFileOffset( struct OpenFiles* files , uint32_t startCluster , uint32_t newOffset ) {

    size_t success = -1;

    for ( size_t i = 0 ; i < 10 ; i++ ) {

        if( startCluster == files->files[i].startCluster ) {
            files->files[i].offset = newOffset;
            success = 0;
            break;
        }
    }

    return success;
}

//returns NULL on not found
OpenFile* getOpenFile( struct OpenFiles* files , uint32_t startCluster , CurrentDirectory* direc , char* filename  ) {

    OpenFile* file = NULL;

    for ( size_t i = 0 ; i < 10 ; i++ ) {

        OpenFile* curFile = &(files->files[i]);

        if( curFile->open == 1 && 
            curFile->startCluster == startCluster && 
            strcmp( curFile->fileName , filename) == 0 && 
            strcmp( curFile->filePath , direc->cwd ) == 0)  {

            file = curFile;
            break;
        }
    }

    return file;
}

