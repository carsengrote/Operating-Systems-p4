#include <stdio.h>
#include <fcntl.h>
#include "udp.h"


struct Checkpoint {
    
    int logEnd;
    int inodeMapPtrs[256];

};

struct Message { 
    char cmd; // a letter representing what command the client wants to execute
    int error; // 0 = no error, -1 = error
    int pinum;
    int inum;
    int block;
    char name[28];
    struct DirEntry;
    struct Stat;
    char buffer[4096];
}

struct Stat { 
    int type; // 0 = Dir, 1 = File
    int size;
};

struct DirEntry {
    
    char name[28];
    int inode;

};

struct Directory { 

   struct DirEntry entries[128]; 

};

struct Inode {

    int size;
    char type; 
    int ptrs[14];

};

// represents a single piece / chunk of the inode map - 16 pointers
struct InodeMapPiece {

    int inodePtrs[16];

};

// represents the whole inode map as an array of 256 inode map pieces,
// each piece is an array of 16 inode ptrs, 256*16 = 4096 inodes
// each entry of the inode map is just a pointer to the disk address
// of that inode
struct InodeMap {
    
    // pointers to each chunk of inode map is currently on disk
    // int inodeMapLocations[256];
    // actual inode map 256 inode map pieces, each piece contains 16 pointers
    struct InodeMapPiece* inodeMapPieces[256];

};


int disk; // file descriptor for the disk

struct Checkpoint* CR; // in memory version of the checkpoint region

struct InodeMap* inodeMap; // in memory version of the inode map

struct Message* clientMsg; // what the client sent over, stored globally for ease

// Creates disk when the one given doesn't exist
void createDisk(char* diskName){
    // creates it if doesn't exist
    disk = open(diskName, O_CREAT, O_RDWR);
    
    int initialCR[257];
    
    initialCR[0] = 0; // to be set at the end! 

    for (int i = 1; i < 257; i++){
        // initializing pointers to parts of the inode map
        initialCR[i] = 1028 + ((i - 1) * (16 * sizeof(int)));
    }

    int emptyInodeMap[4096];

    emptyInodeMap[0] = 17412; // location of root directory inode

    for(int i = 1; i < 4096; i++){
        emptyInodeMap[i] = -1;
    }

    struct Inode* rootInode = malloc(sizeof(struct Inode));
    
    for(int i = 0; i < 14; i++){
        rootInode->ptrs[i] = -1;
    }
    rootInode->size = 4096;
    rootInode->type = 'd';
    rootInode->ptrs[0] = 17473; // location of root data, magic? did the math

    // creating the root directory
    struct Directory* rootDir = malloc(sizeof( struct Directory)); 

    // setting all inodes to -1 in root dir initially
    for (int i = 0; i < 128; i ++){
        rootDir->entries[i].inode = -1;
    }

    // adding the . entry to root dir
    strcpy(rootDir->entries[0].name, ".");
    rootDir->entries[0].inode = 0;

    // adding the .. entry to root dir
    strcpy(rootDir->entries[1].name, "..");
    rootDir->entries[1].inode = 0; 

    write(disk, initialCR, 257 * sizeof(int));
    write(disk, emptyInodeMap, 4096 * sizeof(int));
    write(disk, rootInode, sizeof(struct Inode));
    write(disk, rootDir, sizeof(struct Directory));
    int logEnd = lseek(disk, 0, SEEK_END); // end of the log so far, did the math
    lseek(disk, 0, SEEK_SET); // rewind back to start of disk
    write(disk, &logEnd, sizeof(int)); // write the end of the log to the Checkpoint since first int in the checkpoint is log end
    // now force writes to disk and put disk pointer back to start of the disk
    lseek(disk, 0, SEEK_SET);
    fsync(disk);

    return;
}

// initializes the file system from a given disk or new disk
void initializeDisk(char* diskImage){

    disk = open(diskImage, O_RDWR);
    if (disk < 0){
        createDisk(diskImage);
    }
    lseek(disk, 0, SEEK_SET);
    // read in checkpoint region to in memory version, CR
    CR = malloc(sizeof(struct Checkpoint));
    assert(CR > 0);
    read(disk, &(CR->logEnd), sizeof(int));
    read(disk, &(CR->inodeMapPtrs), 256*sizeof(int));

    // reading in the inode map to the in memory version, inodeMap
    inodeMap = malloc(sizeof(struct InodeMap));
    for(int i = 0; i < 256; i++){
        inodeMap->inodeMapPieces[i] = malloc(sizeof(struct InodeMapPiece));
    }

    for(int i = 0; i < 256; i++){
        // disk address of the current section of the inode map
        int inodeMapSection = CR->inodeMapPtrs[i];
        
        lseek(disk, inodeMapSection, SEEK_SET);

        struct InodeMapPiece* currPiece = inodeMap->inodeMapPieces[i];
        
        for(int j = 0; j < 16; j++){
            
            read(disk, &(currPiece->inodePtrs[j]), sizeof(int));
        }
    }

    lseek(disk, 0, SEEK_SET);
    return;
}





int main(int argc, char* argv[]){
    
    if (argc != 3){
        printf("Usage: server <portnum> <file-system-image>\n");
        exit(0);
    }

    initializeDisk(argv[2]);

    int portNum = atoi(argv[1]);
    if (portNum == 0){
        printf("Port error\n");
        exit(0);
    }
    int sd = UDP_Open(portNum);

    struct sockaddr_in addr;
    clientMsg = malloc(sizeof(struct Message));

    while(1){

        int rc = UDP_Read(sd, &addr, request, 6000);
        if (rc <= 0 ){
            continue;
        } 
        memcpy(cmdChar, clientMsg, sizeof(struct Message));

        // Here check the cmdChar, do different stuff depending on its value
        // Probably make a function for request type,  6 total
        // return the wanted data within the function
        
    }

    return 0;
}
