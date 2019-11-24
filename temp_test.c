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

  for (i = 0; i < MAX_FNAME_LENGTH; i++) {
    if (i != 16) {
      fname[i] = 'A' + (rand() % 26);
    }
    else {
      fname[i] = '.';
    }
  }
  fname[i] = '\0';
  return (strdup(fname));
}

int main(void)
{
    mksfs(1);
    printf("reinitializing...\n");
    mksfs(0);

    DIR_ENTRY dir_entries[10];

    for (int i = 0; i < 10; i++)
    {
        dir_entries[i].filename = rand_name();
        dir_entries[i].inode_num = i+1;

        rdc_insert(dir_entries[i]);
    }

    int inode_num = rdc_get_inode_num(dir_entries[3].filename);

    // remove middle entry
    rdc_remove(dir_entries[5].filename);

    //remove first entry
    rdc_remove(dir_entries[0].filename);

    //remove last entry
    rdc_remove(dir_entries[9].filename);

    printf(":)\n");
}