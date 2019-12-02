#include "sfs_api.h"
#include "root_dir_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FNAME_LENGTH 20

char *rand_name()
{
    char fname[MAX_FNAME_LENGTH];
    int i;

    for (i = 0; i < MAX_FNAME_LENGTH; i++)
    {
        if (i != 16)
        {
            fname[i] = 'A' + (rand() % 26);
        }
        else
        {
            fname[i] = '.';
        }
    } 
    fname[i] = '\0';
    return (strdup(fname));
}

int main(void)
{
    mksfs(1);

    char data[128] = "the brown fox had a crisis and had to go through intensive therapy only to find out that he likes men. what a fucking bummer...";
    char read_buf[128];
    
    char *name = rand_name();
    int fd = sfs_fopen(name);

    for (int i = 0; i < 16; i++)
        sfs_fwrite(fd, data, 128);

    for (int i = 0; i < 16; i++)
    {
        sfs_fread(fd, read_buf, 128);
        printf("read %d: ", i);
        printf("%s\n", read_buf);
    }
    
    sfs_frseek(fd, 960);
    sfs_fread(fd, read_buf, 128);
    printf("weird one: %s\n", read_buf);

    printf(":)\n");
}