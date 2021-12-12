#include <stdio.h>
#include <string.h>
#include "mfs.h"
#include "udp.h"

/*
Commands:
I: init
L: lookup
S: stat
W: write
R: read
C: create
U: unlink
H: shutdown
*/

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

struct Message message; // global message to send back and forth 
struct sockaddr_in addrSnd, addrRcv;
int sd;

int MFS_Init(char *hostname, int port) {
    sd = UDP_Open(20000); //do we need this? Also, I think the port number should be diff
    int rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    // is this what we should be returning?
    return rc;
}

int MFS_Lookup(int pinum, char *name) {

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'L';
    strcpy(message.name, name);
    message.pinum = pinum;

    int rc = UDP_Write(sd, &addrSnd,  (void *)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n");
	    return -1;
    }
    memset(&message, 0, sizeof(struct Message));

    rc = UDP_Read(sd, &addrRcv, (void*) &message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n");
	    return -1;
    }
    if(message.error == -1) {
        return -1;
    }
    return message.inum;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'S';
    message.inum = inum;
    int rc = UDP_Write(sd, &addrSnd, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n");
	    return -1;
    }

    memset(&message, 0, sizeof(struct Message));

    rc = UDP_Read(sd, &addrRcv, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n");
	    return -1;
    }
    if(message.error == -1) {
        return -1;
    }
    m->type = message.stat.type;
    m->size = message.stat.size;
    return 0;
}

int MFS_Write(int inum, char *buffer, int block) {

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'W';
    message.inum = inum;
    strcpy(message.buffer, buffer);
    message.block = block;
    int rc = UDP_Write(sd, &addrSnd,  (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n");
	    return -1;
    }

    memset(&message, 0, sizeof(struct Message));

    rc = UDP_Read(sd, &addrRcv, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n");
	    return -1;
    }
    if(message.error == -1) {
        return -1;
    }

    return 0;
}

int MFS_Read(int inum, char *buffer, int block) {

    memset(&message, 0, sizeof(struct Message));
    // might want to check how the DirEnt is being read into the char buffer
    message.cmd = 'R';
    message.inum = inum;
    message.block = block;
    int rc = UDP_Write(sd, &addrSnd, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n");
	    return -1;
    }

    memset(&message, 0, sizeof(struct Message));

    rc = UDP_Read(sd, &addrRcv,  (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n");
	    return -1;
    }
    if(message.error == -1) {
        return -1;
    }
    strcpy(buffer, message.buffer);

    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {

    memset(&message, 0, sizeof(struct Message));
    if (strlen(name) > 28){
        return -1;
    }
    message.cmd = 'C';
    message.pinum = pinum;
    message.type = type;
    strcpy(message.name, name);
    int rc = UDP_Write(sd, &addrSnd, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n");
	    return -1;
    }

    memset(&message, 0, sizeof(struct Message));

    rc = UDP_Read(sd, &addrRcv, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n");
	    return -1;
    }
    if(message.error == -1) {
        return -1;
    }

    return 0;
}

int MFS_Unlink(int pinum, char *name) {

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'U';
    message.pinum = pinum;
    strcpy(message.name, name);
    int rc = UDP_Write(sd, &addrSnd, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n");
	    return -1;
    }

    memset(&message, 0, sizeof(struct Message));

    rc = UDP_Read(sd, &addrRcv, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n");
	    return -1;
    }
    if(message.error == -1) {
        return -1;
    }

    return 0;
}

int MFS_Shutdown() {

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'H';
    int rc = UDP_Write(sd, &addrSnd, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n");
	    return -1;
    }
    memset(&message, 0, sizeof(struct Message));

    rc = UDP_Read(sd, &addrRcv, (void*)&message, sizeof(struct Message));
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n");
	    return -1;
    }
    if(message.error == -1) {
        return -1;
    }

    return 0;
}

