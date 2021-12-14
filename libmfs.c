#include <stdio.h>
#include <string.h>
#include "mfs.h"
#include "udp.h"
#include <sys/time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

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
struct timeval tv;

int MFS_Init(char *hostname, int port) {

    int port_num = 20000;
    sd = UDP_Open(port_num);
   
    while(sd == -1) {
        port_num+=1;
        sd = UDP_Open(port_num);
    }

    int rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    return rc;
}

int MFS_Lookup(int pinum, char *name) {
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set rfds;
    int retval;

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'L';
    strcpy(message.name, name);
    message.pinum = pinum;

    FD_ZERO(&rfds);
    FD_SET(sd, &rfds);
    int rc = UDP_Write(sd, &addrSnd,  (void *)&message, sizeof(struct Message));
    while((retval = select(sd+1, &rfds, NULL, NULL, &tv)) == 0) {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rc = UDP_Write(sd, &addrSnd, (void *)&message, sizeof(struct Message));
    }

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
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set rfds;
    int retval;

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'S';
    message.inum = inum;

    FD_ZERO(&rfds);
    FD_SET(sd, &rfds);
    int rc = UDP_Write(sd, &addrSnd,  (void *)&message, sizeof(struct Message));
    while((retval = select(sd+1, &rfds, NULL, NULL, &tv)) == 0) {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rc = UDP_Write(sd, &addrSnd, (void *)&message, sizeof(struct Message));
    }

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
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set rfds;
    int retval;

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'W';
    message.inum = inum;
    memcpy(message.buffer, buffer, 4096);
    message.block = block;

    FD_ZERO(&rfds);
    FD_SET(sd, &rfds);
    int rc = UDP_Write(sd, &addrSnd,  (void *)&message, sizeof(struct Message));
    while((retval = select(sd+1, &rfds, NULL, NULL, &tv)) == 0) {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rc = UDP_Write(sd, &addrSnd, (void *)&message, sizeof(struct Message));
    }

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
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set rfds;
    int retval;

    memset(&message, 0, sizeof(struct Message));
    // might want to check how the DirEnt is being read into the char buffer
    message.cmd = 'R';
    message.inum = inum;
    message.block = block;
    
    FD_ZERO(&rfds);
    FD_SET(sd, &rfds);
    int rc = UDP_Write(sd, &addrSnd,  (void *)&message, sizeof(struct Message));
    while((retval = select(sd+1, &rfds, NULL, NULL, &tv)) == 0) {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rc = UDP_Write(sd, &addrSnd, (void *)&message, sizeof(struct Message));
    }

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

    memcpy(buffer, message.buffer, 4096);

    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set rfds;
    int retval;

    memset(&message, 0, sizeof(struct Message));
    if (strlen(name) > 28){
        return -1;
    }
    message.cmd = 'C';
    message.pinum = pinum;
    message.type = type;
    strcpy(message.name, name);

    FD_ZERO(&rfds);
    FD_SET(sd, &rfds);
    int rc = UDP_Write(sd, &addrSnd,  (void *)&message, sizeof(struct Message));
    while((retval = select(sd+1, &rfds, NULL, NULL, &tv)) == 0) {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rc = UDP_Write(sd, &addrSnd, (void *)&message, sizeof(struct Message));
    }

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
    
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    fd_set rfds;
    int retval;

    memset(&message, 0, sizeof(struct Message));
    message.cmd = 'U';
    message.pinum = pinum;
    strcpy(message.name, name);
    
    FD_ZERO(&rfds);
    FD_SET(sd, &rfds);
    int rc = UDP_Write(sd, &addrSnd,  (void *)&message, sizeof(struct Message));
    while((retval = select(sd+1, &rfds, NULL, NULL, &tv)) == 0) {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        rc = UDP_Write(sd, &addrSnd, (void *)&message, sizeof(struct Message));
    }

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
