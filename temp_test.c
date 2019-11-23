#include "sfs_api.h"
#include <stdio.h>

int main(void)
{
    mksfs(1);
    printf("reinitializing...\n");
    mksfs(0);
}