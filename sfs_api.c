#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct SUPER_BLOCK {
    int magic_number;
    int block_size;
    int fs_size;
    int inode_table_length;
    int root_dir_inode_num;
} SUPER_BLOCK;

// padded to 128 bytes
typedef struct INODE {
    int valid;
    int mode; /* not used */
    int link_count;
    int uid; /* not used */
    int gid; /* not used */
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

INODE inode_table_cache[512];

void mksfs(int fresh) 
{
    int block_size = 1024;
    int num_blocks = 8258; // 1 super block
                           // 64 blocks for inode table
                           // 8192 data blocks
                           // 1 free bitmap block
    int inode_table_length = 64;
    SUPER_BLOCK super_block;
    INODE root_dir_inode;

    if (fresh)
    {
        // init with 8 megabytes of free space
        init_fresh_disk("emulated_disk", block_size, num_blocks);

        // write super block to first block of disk
        super_block.magic_number = 0xABCD0005;
        super_block.block_size = block_size;
        super_block.fs_size = num_blocks;
        super_block.inode_table_length = inode_table_length;
        super_block.root_dir_inode_num = 0;
        write_blocks(0, 1, &super_block);

        // write root directory to first inode entry
        root_dir_inode.valid = 1;
        root_dir_inode.mode = 0;
        root_dir_inode.link_count = 1;
        root_dir_inode.uid = 0;
        root_dir_inode.gid = 0;
        root_dir_inode.size = 0;
        write_blocks(1, 1, &root_dir_inode);
    }
    else
    {
        init_disk("emulated_disk", block_size, num_blocks);

        // read super block
        void *super_block_buff = (void*) malloc(block_size); // 
        read_blocks(0, 1, super_block_buff);
        super_block = *((SUPER_BLOCK*) super_block_buff);
        free(super_block_buff);

        // validate super block
        if (super_block.magic_number != 0xABCD0005)
        {
            printf("error reading magic number in super block\n");
            exit(1);
        }
    }

    // cache inode table
    read_blocks(1, 64, inode_table_cache);

    // get root inode
    root_dir_inode = inode_table_cache[super_block.root_dir_inode_num];

    // cache root directory
}

int sfs_getnextfilename(char *fname)
{
    return 1;
}

int sfs_getfilesize(const char* path)
{
    return 1;
}

int sfs_fopen(char *name)
{
    return 1;
}

int sfs_fclose(int fileID)
{
    return 1;
}

int sfs_frseek(int fileID, int loc)
{
    return 1;
}

int sfs_fwseek(int fileID, int loc)
{
    return 1;
}

int sfs_fwrite(int fileID, char *buf, int length)
{
    return 1;
}

int sfs_fread(int fileID, char *buf, int length)
{
    return 1;
}

int sfs_remove(char *file)
{
    return 1; 
}
