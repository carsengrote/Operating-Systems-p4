#include <stdio.h>
#include "mfs.h"

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

struct Message* message; // global message to send back and forth 
struct sockaddr_in addrSnd, addrRcv;

int MFS_Init(char *hostname, int port) {
     // int sd = UDP_Open(port); do we need this? Also, I think the port number should be diff
    int rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    // is this what we should be returning?
    return rc;
}

int MFS_Lookup(int pinum, char *name) {
    message->cmd = 'L';
    strcpy(message->name, name);
    message->pinum = pinum;

    int rc = UDP_Write(sd, &addrSnd, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n")
	    return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n")
	    return -1;
    }
    if(message->error == -1) {
        return -1;
    }
    return message->inum;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    mesasge->cmd = 'S';
    message->inum = inum;
    int rc = UDP_Write(sd, &addrSnd, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n")
	    return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n")
	    return -1;
    }
    if(message->error == -1) {
        return -1;
    }
    m->type = message->stat.type;
    m->size = message->stat.size;
    return 0;
}

int MFS_Write(int inum, char *buffer, int block) {
    mesasge->cmd = 'W';
    message->inum = inum;
    strcpy(message->buffer, buffer);
    message->block = block;
    int rc = UDP_Write(sd, &addrSnd, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n")
	    return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n")
	    return -1;
    }
    if(message->error == -1) {
        return -1;
    }

    return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
    // might want to check how the DirEnt is being read into the char buffer
    mesasge->cmd = 'R';
    message->inum = inum;
    message->block = block;
    int rc = UDP_Write(sd, &addrSnd, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n")
	    return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n")
	    return -1;
    }
    if(message->error == -1) {
        return -1;
    }
    strcpy(buffer, message->buffer);

    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
    mesasge->cmd = 'C';
    message->pinum = pinum;
    message->type = type;
    strcpy(message->name, name);
    int rc = UDP_Write(sd, &addrSnd, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n")
	    return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n")
	    return -1;
    }
    if(message->error == -1) {
        return -1;
    }
    strcpy(buffer, message->buffer);

    return 0;
}

int MFS_Unlink(int pinum, char *name) {
    mesasge->cmd = 'U';
    message->pinum = pinum;
    strcpy(message->name, name);
    int rc = UDP_Write(sd, &addrSnd, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n")
	    return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n")
	    return -1;
    }
    if(message->error == -1) {
        return -1;
    }

    return 0;
}

int MFS_Shutdown() {
    mesasge->cmd = 'H';
    int rc = UDP_Write(sd, &addrSnd, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Write is -1\n")
	    return -1;
    }

    rc = UDP_Read(sd, &addrRcv, (void *) message, BUFFER_SIZE);
    if (rc < 0) {
        printf("return code of UDP_Read is -1\n")
	    return -1;
    }
    if(message->error == -1) {
        return -1;
    }

    return 0;
}
