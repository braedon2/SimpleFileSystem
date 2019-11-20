// functions used by sfs_api

#include "disk_emu.h"

typedef struct {
    int magic_number;
    int block_size;
    int fs_size;
    int inode_table_length;
    int root_dir_inode_num;
} SUPER_BLOCK;

typedef struct {
    int valid;
    int mode;
    int link_count;
    int uid;
    int gid;
    int size;
    int direct_ptr[12];
    int *ind_ptr;
    int : 32; // int comes first for proper alignment
    long long : 64;
    long long : 64;
    long long : 64;
    long long : 64;
    long long : 64;
} INODE;

void formatFreshDisk();