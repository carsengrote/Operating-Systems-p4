#include <stdio.h>
#include "mfs.h"
#include <stdlib.h>
#include <string.h>

struct St {
    int a;
    int b;
    char c;
};


int main(int argc, char*argv[]){
    int rc = MFS_Init("localhost", 10000);
    char* out = malloc(4098);
    
    rc = MFS_Creat(0,1,"file");
    printf("file creat rc: %d\n", rc);
    rc = MFS_Creat(0,1,"file2");
    printf("file creat rc: %d\n", rc);
    rc = MFS_Creat(0,1,"file3");
    printf("file creat rc: %d\n", rc);
    rc = MFS_Creat(0,1,"file4");
    printf("file creat rc: %d\n", rc);

    int inode = MFS_Lookup(0, "file3");

    printf("file inode num: %d\n", inode);

    char data[50] = "Hello dommmy";

    rc = MFS_Write(inode,data, 2);

    printf("write rc: %d\n", rc);

    rc = MFS_Read(inode, out, 2);

    printf("Read rc: %d\n", rc);

    printf("read data: %s\n", out);

    return 0;

}
