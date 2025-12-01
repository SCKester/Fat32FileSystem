#include <string.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct { 
    char fileName[12]; // null terminated file name
    char* filePath; //dynamically allocated MUST BE FREED
    char permissions[3]; //can be any combination of rw , wr , r , w , is null terminated
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
size_t openFile( struct OpenFiles* files ,  char* fileName , char* permisssions , uint32_t startCluster , char* path );

//closes file with starting cluster of StartCluster, returns index of closed file or -1 if not found
size_t closeFile( struct OpenFiles* files , uint32_t startCluster );

void closeAllFiles( struct OpenFiles* files );