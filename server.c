#include <stdio.h>
#include <fcntl.h>
#include "udp.h"


struct Checkpoint {
    
    int logEnd;
    int inodeMapPtrs[256];

};

struct DirEntry {
    
    char name[28];
    int inode;

};

struct Stat { 
    int type; // 0 = Dir, 1 = File
    int size;
};

struct Message { 
    char cmd; // a letter representing what command the client wants to execute
    int error; // 0 = no error, -1 = error
    int pinum;
    int inum;
    int block;
    int type;
    char name[28];
    struct DirEntry dirEntry;
    struct Stat stat;
    char buffer[4096];
};



struct Directory { 

   struct DirEntry entries[128]; 

};

struct Inode {

    int size;
    char type; 
    int ptrs[14];

};

// whole inode map is just an array of 4096 disk addresses, each one is the address of an inode
struct InodeMap {

    int inodePtrs[4096];
};

int disk; // file descriptor for the disk

struct Checkpoint* CR; // in memory version of the checkpoint region

struct InodeMap* inodeMap; // in memory version of the inode map

struct Message* clientMsg; // what the client sent over, stored globally for ease
struct Message* replyMsg;

int sd; // socket descriptor
struct sockaddr_in addr; // sock struct

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

    // writing all this stuff to the disk image now
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
    // creates disk image if it don't exist
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
        // disk address of the current section of the inode map
        int inodeMapSection = CR->inodeMapPtrs[i];

        lseek(disk, inodeMapSection, SEEK_SET);
        
        for (int j = 0; j < 16; j++){
            read(disk, &(inodeMap->inodePtrs[(i*16) + j]), sizeof(int));
        }
    }

    lseek(disk, 0, SEEK_SET);
    return;
}

void sendReply(){
    UDP_Write(sd, &addr, (void *)replyMsg, sizeof(struct Message));
}

void lookup(){
    
    int pinum = clientMsg->pinum;
    char childName[28];
    // getting the name of the file we're looking for
    strcpy(childName, clientMsg->name); 

    replyMsg->error = 0;
    // Using in memory inode map to get the disk address of the parent inode
    int inodeAddr = inodeMap->inodePtrs[pinum];
    // if it's not valid send reply and return
    if (inodeAddr == -1){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // seeking to parent inode 
    lseek(disk, inodeAddr, SEEK_SET);
    struct Inode parentInode;
    // reading in parent inode
    read(disk, &parentInode, sizeof(struct Inode)); 

    int found = 0; // keeping track whether it's found or not
    
    for(int dataBlk = 0; dataBlk < 14; dataBlk ++){
        // getting address of current block in inode
        int currentBlkAddr = parentInode.ptrs[dataBlk];
        // check if current Block pointer is valid
        if (currentBlkAddr == -1){
            continue; 
        }
        // seeking to it if it's valid
        lseek(disk, currentBlkAddr, SEEK_SET);
        struct Directory parentDir;
        read(disk, &parentDir, sizeof(struct Directory));
        // now reading through entries in the directory
        for (int dirEntryIndex = 0; dirEntryIndex < 128; dirEntryIndex ++){
            
            // current directory entry is not in use
            if(parentDir.entries[dirEntryIndex].inode == -1){
                continue; // entry not in use
            }

            if(strcmp(parentDir.entries[dirEntryIndex].name, childName) == 0){
                // found that boy lets go
                replyMsg->inum = parentDir.entries[dirEntryIndex].inode;
                found = 1;
            }
        }
    }

    // file not found so send back error
    if (found == 0){
        replyMsg->error = -1;
    }

    // now we write our reply to the socket
    sendReply();

    return;
}

void stat(){

    replyMsg->error = 0;
    // what we're looking for
    int inum = clientMsg->inum;
    // need to find this inode
    int inodeAddr = inodeMap->inodePtrs[inum];

    // inode don't exist
    if(inodeAddr == -1){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // seek to address of inode
    lseek(disk, inodeAddr, SEEK_SET);
    // read the inode on the disk into a struct on the stack
    struct Inode inode;
    read(disk, &inode, sizeof(struct Inode));
    // get that info we need
    if (inode.type == 'f'){
        replyMsg->stat.type = 1;  
    } else {
        replyMsg->stat.type = 0;   
    }
    replyMsg->stat.size = inode.size;

    sendReply(); 

    return;
}

void writeLog(){

    replyMsg->error = 0;
    // getting client given arguments
    int inum = clientMsg->inum;
    char* writeBuffer = strdup(clientMsg->buffer);
    int writeBlock = clientMsg->block;

    // check for invalid block to write to
    if (writeBlock < 0 || writeBlock > 13){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // look for inode
    int inodeAddr = inodeMap->inodePtrs[inum];
    // send error if invalid inode
    if (inodeAddr == -1){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // seek disk to inode
    lseek(disk, inodeAddr, SEEK_SET);
    struct Inode inode; 
    // read in inode
    read(disk, &inode, sizeof(struct Inode));
    // error if inode is a directory not file
    if (inode.type == 'd'){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // calculating the last block allocated by the file size
    int lastBlockIndex = (int(inode.size / 4096)) - 1;
    // updating size if it's writing to a new block
    if (writeBlock > lastBlockIndex){
        inode.size = inode.size + ((writeBlock - lastBlockIndex) * 4096);
    }

    // need to write new data block, new inode, new piece of inode map to disk (log)
    // so first let's calculate the addresses of where all this will be written
    int newBlockAddr = CR->logEnd;
    int newInodeAddr = newBlockAddr + 4096;
    int newInodeMapPieceAddr = newInodeAddr + 61; 
    int newLogEnd = newInodeMapPieceAddr + 64;

    // making the new inode map piece
    int inodeMapPieceStart = (int) (inum / 16);
    int newInodeMapPiece[16];
    // copying all inode addrs to new piece from inode amp
    for (int i = 0; i < 16; i ++){
        newInodeMapPiece[i] = inodeMap->inodePtrs[(inodeMapPieceStart * 16) + i];
    }
    // setting the new inode ptr to the new inode
    newInodeMapPiece[inum % 16] = newInodeAddr;

    // update inode pointers to the new block
    inode.ptrs[writeBlock] = newBlockAddr;

    // seeking to log end to write
    lseek(disk, newBlockAddr, SEEK_SET);
    // writing the new data block to log
    write(disk, writeBuffer, 4096);
    // writing updated inode to log
    write(disk, &inode, sizeof(struct Inode));
    // writing new piece of inode map
    write(disk, &newInodeMapPiece, 64); 

    // now need to update CR, in memory CR, and in memory inode map
    // updating in memory CR
    CR->inodeMapPtrs[inodeMapPieceStart] = newInodeMapPieceAddr;
    CR->logEnd = newLogEnd;
    // updating the inode map piece poitner in the on disk CR
    lseek(disk, 0, SEEK_SET);
    write(disk, &newLogEnd, sizeof(int));
    lseek(disk, sizeof(int) + (sizeof(int)* inodeMapPieceStart), SEEK_SET);
    write(disk, &newInodeMapPieceAddr, sizeof(int));
    // updating in memory inode map
    inodeMap->inodePtrs[inum] = newInodeAddr;
    
    // forcing writes to disk
    fsync(disk); 

    // write completed, ensure errror code is 0 and reply to client
    replyMsg->error = 0;
    sendReply();

    return;
}

void diskRead(){

    replyMsg->error = 0;

    int inum = clientMsg->inum;
    int block = clientMsg->block;

    inodeAddr = inodeMap->inodePtrs[inum];

    if (inodeAddr == -1){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    lseek(disk, inodeAddr, SEEK_SET);
    struct Inode inode;
    read(disk, &inode, sizeof(struct Inode));

    int fileSize = inode.size; 
    int lastBlockIndex = (int(inode.size / 4096)) - 1; 

    // Data to send back
    char data[4096];
    // empty block inbetween allocated blocks
    if ((block <= lastBlockIndex) && (inode.ptrs[block] == -1)){
        // send back all zeros
        memset(data,0,4096);
        strcpy(replyMsg->buffer, data);
        replyMsg->error = 0;
        sendReply();
        return;
    
    } else if (inode.ptrs[block] == -1){
        // invalid block 
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // if here then it's valid data in an allocated block to read and send back

    int blockAddr = inode.ptrs[block];
    lseek(disk, blockAddr, SEEK _SET);
    char data[4096];
    read(disk, &data, 4096);

    strcpy(replyMsg->buffer, data);
    replyMsg->error = 0;
    sendReply();
    return;
}

void create() {
    ///type 0 = file and 1 = dir
    replyMsg->error = 0;

    int newInum = -1;
    //get new inode
    for(int index = 0; index < 4096; index++) {
        // find the first inode that has not been allocated yet
        if(inodeMap->inodePtrs[index] == -1) {
            newInum = index;
            break;
        }
    }


    // Inode map is full, return error
    if(newInum == -1) {
        replyMsg->error = -1;
        sendReply();
        return;
    }
    // store new inode number in message
    replyMsg->inum = newInum;

    int pinum = clientMsg->pinum;

    int pInodeAddr = inodeMap->inodePtrs[pinum];

    if (pInodeAddr == -1){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    lseek(disk, pInodeAddr, SEEK_SET);
    struct Inode pinode;
    read(disk, &pinode, sizeof(struct Inode));

    // error if parent is a file
    if(pinode.type == 'f') {
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // update parent directory
    int pinodeDataPtr = -1;
    for(int index=0; index < 14; index++) {
        if(pinode->ptrs[index] == -1) {
            // if no pointers in pinode are allocated - SHOULDN't be a case
            pinodeDataPtr = index-1;
            break;
        }
    }
    // what if all the data blocks are allocated? - set data ptr to last data block
    if(pinodeDataPtr == -1) {
        pinodeDataPtr = 13;
    }
    //check to see if new dir block must be created
    int newDirBlock = 0;
    struct Directory pdir;
    lseek(disk, pinode->ptrs[pinodeDataPtr], SEEK_SET);
    read(disk, &pdir, sizeof(struct Directory));
    int newDirEntryIndex = -1;
    for(int index = 0; index < 128; index++) {
        if(pdir.entries[index] == -1) {
            newDirEntryIndex = index;
            break;
        }
    }
    // Directory is full and we are using the last data block, return error
    if(newDirEntryIndex == -1 && pinodeDataPtr == 13) {
        replyMsg->error = -1;
        sendReply();
        return;
    }
    else if(newDirEntryIndex == -1) {
        newDirBlock = 1;
        // want to allocate new data block here and make a new directory entry in there
        struct Directory pdir;
        // setting all inodes to -1 in dir initially
        for (int i = 0; i < 128; i ++){
            pdir.entries[i].inode = -1;
        }
        newDirEntryIndex = 0;
        pinode->size += 4096;
    }   

    //make new Dir Entry
    struct DirEntry newEntry;
    strcpy(newEntry.name, clientMsg->name);
    newEntry.inode = newInum;
    // set new entry
    pdir.entries[newDirEntryIndex] = newEntry;
    
    //make new inode
    struct Inode newInode;
    
    for(int i = 0; i < 14; i++){
        newInode.ptrs[i] = -1;
    }
    if(clientMsg->type == 0) {
        newInode.type = 'f';
        //Find out what else to do for file creation
        newInode.size=0;
    }
    else {
        newInode.type = 'd';
        newInode.size=4096;
        

        // creating the directory
        struct Directory newDir; 

        // setting all inodes to -1 in dir initially
        for (int i = 0; i < 128; i ++){
            newDir.entries[i].inode = -1;
        }

        // adding the . entry to dir
        strcpy(newDir.entries[0].name, ".");
        newDir.entries[0].inode = newInum;

        // adding the .. entry to root dir
        strcpy(newDir.entries[1].name, "..");
        newDir.entries[1].inode = pinum; 
    }
    // Find piece of inode map in CR region




    // Addr Math
    if(newInode.type == 'd' && newDirBlock == 1) {
        int newDirAddr = CR->logEnd;
        int newInodeAddr = newDirAddr+4096;
        int inodeMapAddr = newInodeAddr + 61;
        int pDirBlockAddr = inodeMapAddr + 64;
        int pInodeAddr = pDirBlockAddr + 4096;
        int newLogEnd = pInodeAddr + 61;
        // update inode pointers to the new block
        newInode.ptrs[0] = newDirAddr // Log end
    }
    else if(newInode.type == 'd' && newDirBlock == 0){
        int newDirAddr = CR->logEnd;
        int newInodeAddr = newDirAddr+4096;
        int inodeMapAddr = newInodeAddr + 61;
        int pDirBlockAddr = inodeMapAddr + 64;
        int newLogEnd = pDirBlockAddr + 4096;
        // update inode pointers to the new block
        newInode.ptrs[0] = newDirAddr // Log end
    }
    else if(newInode.type == 'f' && newDirBlock == 1) {
        int newInodeAddr = CR->logEnd;
        int inodeMapAddr = newInodeAddr + 61;
        int pDirBlockAddr = inodeMapAddr + 64;
        int pInodeAddr = pDirBlockAddr + 4096;
        int newLogEnd = pInodeAddr + 61;
    }
    else {
        int newInodeAddr = CR->logEnd;
        int inodeMapAddr = newInodeAddr + 61;
        int pDirBlockAddr = inodeMapAddr + 64;
        int newLogEnd = pDirBlockAddr + 4096;
    }
    
    // making the new inode map piece
    int inodeMapPieceStart = (int) (newInum / 16);
    int newInodeMapPiece[16];
    // copying all inode addrs to new piece from inode amp
    for (int i = 0; i < 16; i ++){
        newInodeMapPiece[i] = inodeMap->inodePtrs[(inodeMapPieceStart * 16) + i];
    }
    // setting the new inode ptr to the new inode
    newInodeMapPiece[newInum % 16] = newInodeAddr;

    if(newInode.type == 'd' && newDirBlock == 1) {
        // seeking to log end to write
        lseek(disk, newBlockAddr, SEEK_SET);
        // writing the new data block to log
        write(disk, newDir, 4096);
        // writing new inode to log
        write(disk, &newInode, sizeof(struct Inode));
        // writing new piece of inode map
        write(disk, &newInodeMapPiece, 64); 
        // writing new parent directory block
        write(disk, &pdir, 4096); 
        // writing updated parent inode block
        write(disk, &pinode, sizeof(struct Inode)); 
    }
    else if(newInode.type == 'd' && newDirBlock == 0){
        // seeking to log end to write
        lseek(disk, newBlockAddr, SEEK_SET);
        // writing the new data block to log
        write(disk, newDir, 4096);
        // writing new inode to log
        write(disk, &newInode, sizeof(struct Inode));
        // writing new piece of inode map
        write(disk, &newInodeMapPiece, 64); 
        // writing new parent directory block
        write(disk, &pdir, 4096); 
    }
    else if(newInode.type == 'f' && newDirBlock == 1) {
        // seeking to log end to write
        lseek(disk, newInodeAddr, SEEK_SET);
        // writing new inode to log
        write(disk, &newInode, sizeof(struct Inode));
        // writing new piece of inode map
        write(disk, &newInodeMapPiece, 64); 
        // writing new parent directory block
        write(disk, &pdir, 4096); 
        // writing updated parent inode block
        write(disk, &pinode, sizeof(struct Inode)); 
    }
    else {
        // seeking to log end to write
        lseek(disk, newInodeAddr, SEEK_SET);
        // writing new inode to log
        write(disk, &newInode, sizeof(struct Inode));
        // writing new piece of inode map
        write(disk, &newInodeMapPiece, 64); 
        // writing new parent directory block
        write(disk, &pdir, 4096); 
    }

    // now need to update CR, in memory CR, and in memory inode map
    // updating in memory CR
    CR->inodeMapPtrs[inodeMapPieceStart] = newInodeMapPieceAddr;
    CR->logEnd = newLogEnd;
    // updating the inode map piece poitner in the on disk CR
    lseek(disk, 0, SEEK_SET);
    write(disk, &newLogEnd, sizeof(int));
    lseek(disk, sizeof(int) + (sizeof(int)* inodeMapPieceStart), SEEK_SET);
    write(disk, &newInodeMapPieceAddr, sizeof(int));
    // updating in memory inode map
    inodeMap->inodePtrs[newInum] = newInodeAddr;
    
    // forcing writes to disk
    fsync(disk); 



    // writes
    // must rewrite piece of inode
    // update CR
    // update in memory version of inode map
    // write at log end
}

void unlink(){

    replyMsg->error = 0;
    int pinum = clientMsg->pinum;
    char name[28];
    strcpy(name, clientMsg->name);

    // address of parent inode
    parentInodeAddr = inodeMap->inodePtrs[pinum];

    // checking it's valid
    if (parentInodeAddr == -1){
        replyMsg->error = -1;
        sendReply();
        return;
    }

    // seek to and read parent inode
    struct Inode parentInode;
    lseek(disk, parentInodeAddr, SEEK_SET); 
    read(disk, &parentInode, sizeof(struct Inode));


    int finalInode = -1;
    int finalParentBlockNumber = -1;
    int finalParentBlockAddr = -1;
    int finalDirIndex = -1;
    int onlyEntryBool = 0;

    // read parent data blocks for directory entry of the child we're looking for 

    int currentDataBlock = 0; 
    int currentDataBlockAddr;
    struct Directory currDirBlock;
    int found = 0; 
    while(((currentDataBlockAddr = parentInode.ptrs[currentDataBlock]) != -1) && (found == 0)){
       
        // seek to and read data block for child we're looking for
        lseek(disk, currentDataBlockAddr, SEEK_SET);
        read(disk, &currDirBlock, sizeof(struct Directory));

        // Now we're in a data block, look through the directory entires in the data block
        int curDirEntryIndex = 0; 
        struct DirEntry curDirEntry = currDirBlock.entries[0];
        while((curDirEntry.inode != -1) && (found == 0)){
            
            // found it! 
            if(strcmp(curDirEntry.name, name) == 0){
                
                finalInode = curDirEntry.inode;
                finalDirIndex = curDirEntryIndex;
                finalParentBlockAddr = currentDataBlockAddr;
                finalParentBlockNumber = currentDataBlock;
                if (finalDirIndex == 0){
                    lastEntryBool = 1;
                }
                found = 1;
            }

            // going to the next directory entry
            curDirEntryIndex++;
            curDirEntry = currDirBlock.entries[curDirEntryIndex]; 
        }
        currentDataBlock++;
    }

    if (found == 0){
        // file / dir not found in parent - not an error since it's a delete so just reply with success
        replyMsg->error = 0;
        sendReply();
        return;
    }

    // look into the found file or directory
    int finalInodeAddress = inodeMap->inodePtrs[finalInode];
    struct Inode inode;
    lseek(disk, finalInodeAddress, SEEK_SET); 
    read(disk, &inode, sizeof(inode));
    
    
    // Check if child is a dir - if it is then ensure it's empty
    if (inode.type == 'd'){
        int firstDataBlockAddr = inode.ptrs[0];
        lseek(disk, firstDataBlockAddr, SEEK_SET);
        struct Directory firstBlock;
        read(disk, &firstBlock, sizeof(struct Directory));
        // first two entries will be self and parent, check third entry
        if (firstBlock.entries[2].inode != -1){
            // error - dir not empty
            replyMsg->error = -1;
            sendReply();
            return;
        }
    }
    
    // now we know either it's an empty Dir or a file so we good to delete

    // marking the directory entry as unused
    currDirBlock.entries[finalDirIndex] = -1;
    // checking if it's the only entry in that block of the directory - then remove that data block from parent
    if (lastEntryBool == 1){
        parentInode.ptrs[finalParentBlockNumber] = -1;
        parentInode.size = parentInode.size - 4096;
    }

    // rewrite piece of inode map for child inode being deleted
    int childInodePiece = int(finalInode / 16);
    int newChildInodePiece[16];
    int childInodePieceAddr = CR->inodeMapPtrs[childInodePiece];
    lseek(disk, childInodePieceAddr, SEEK_SET);
    read(disk, &newChildInodePiece, 16*sizeof(int));
    newChildInodePiece[finalInode % 16] = -1;
    // now can write piece of inode map for child inode now deleted
    lseek(disk, CR->logEnd, SEEK_SET);
    write(disk,&newChildInodePiece, 16*sizeof(int));
    // update CR on disk 
    lseek(disk, (childInodePiece + 1 )*4, SEEK_SET);
    write(disk, &(CR->logEnd), sizeof(int));
    // update in memory CR
    CR->inodeMapPtrs[childInodePiece] = CR->logEnd;
    // update in memory inode map
    inodeMap->inodePtrs[finalInode] = -1;
    // update in memory log end
    CR->logEnd = CR->logEnd + (16*sizeof(int));

    // rewrite parent data block if there's still entries in it
    if (lastEntryBool != 1){

        lseek(disk, CR->logEnd, SEEK_SET);
        write(disk, &currDirBlock, sizeof(struct Directory));
        // update parent inode's pointers to data block
        parentInode.ptrs[finalParentBlockNumber] = CR->logEnd;
        // update in memory log end
        CR->logEnd = CR->logEnd + sizeof(struct Directory));
    }

    // now rewrite parent inode and inode map piece
    lseek(disk, CR->logEnd, SEEK_SET);
    write(disk, &parentInode, sizeof(struct Inode));
    
    // update in memory inode map
    inodeMap->inodePtrs[pinum] = CR->logEnd;
    // update disk inode map piece and pointer
    int parentInodeMapPiece[16];
    // finding the parent piece of the inode map on disk
    int parentInodeMapPieceAddr = CR->inodeMapPtrs[int(pinum / 16)];
    lseek(disk, parentInodeMapPieceAddr, SEEK_SET);
    read(disk, &parentInodeMapPiece, 16*sizeof(int));
    // updating parent inode map piece
    parentInodeMapPiece[(pinum % 16)] = CR->logEnd
    CR->logEnd = CR->logEnd + sizeof(struct Inode);
    lseek(disk, CR->logEnd, SEEK_SET);
    // writing new piece of inode map for parent to disk
    write(disk, &parentInodeMapPiece, 16 * sizeof(int));
    // update in memory CR
    CR->inodeMapPtrs[int(pinum / 16)] = CR->logEnd;
    // update disk CR
    lseek(disk,(int(pinum / 16) + 1)*4, SEEK_SET);
    write(disk, &(CR->logEnd), sizeof(int));

    // update in memory log end
    CR->logEnd = CR->logEnd + (16*sizeof(int));

    // lastly update on disk log end
    lseek(disk, 0, SEEK_SET);
    write(disk, &(CR->logEnd), sizeof(int));

    // force all writes to disk
    fsync(disk);

    // send reply and return
    replyMsg->error = 0;
    sendReply();

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
    sd = UDP_Open(portNum);

    clientMsg = malloc(sizeof(struct Message)); // one for from the client
    replyMsg = malloc(sizeof(struct Message)); // the one we'll send back to the client
    // zero out mem before it's used and set
    memset(clientMsg, 0, sizeof(struct Message));
    memset(replyMsg, 0, sizeof(struct Message));

    while(1){

        int rc = UDP_Read(sd, &addr, (void *)clientMsg, sizeof(struct Message));

        if (rc <= 0 ){
            continue;
        }

        // Here check the cmdChar, do different stuff depending on its value
        // Probably make a function for request type,  6 total
        // return the wanted data within the function
        
        if (clientMsg->cmd == 'L'){
            lookup();
        } else if (clientMsg->cmd == 'S'){
            stat();
        } else if (clientMsg->cmd == 'W'){
            writeLog();
        } else if (clientMsg->cmd == 'R'){
            diskRead();
        } else if (clientMsg->cmd == 'U'){
            unlink();   
        } else if (clientMsg->cmd == 'C'){
            create();   
        }

        memset(clientMsg, 0, sizeof(struct Message));
        memset(replyMsg, 0, sizeof(struct Message));
    }

    return 0;
}
