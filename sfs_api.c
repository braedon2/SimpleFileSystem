#include "sfs_api.h"
#include "disk_emu.h"
#include "root_dir_cache.h"
#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 8258 // 1 super block
                        // 64 blocks for inode table
                        // 8192 data blocks
                        // 1 free bitmap block
#define INODE_TABLE_LENGTH 64
#define ROOT_DIR_INODE_NUM 0
#define MAX_OPEN_FILES 100

void init_open_file_descriptor_table();

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

typedef struct OPEN_FILE_DESCRIPTOR_TABLE_ENTRY {
    int valid;
    int inode_num;
    int rptr;
    int wptr;
} OPEN_FILE_DESCRIPTOR_TABLE_ENTRY;

OPEN_FILE_DESCRIPTOR_TABLE_ENTRY open_file_descriptor_table[MAX_OPEN_FILES];

void mksfs(int fresh) 
{
    INODE root_dir_inode;
    SUPER_BLOCK super_block;

    if (fresh)
    {
        // init with 8 megabytes of free space
        init_fresh_disk("emulated_disk", BLOCK_SIZE, NUM_BLOCKS);

        // write super block to first block of disk
        super_block.magic_number = 0xABCD0005;
        super_block.block_size = BLOCK_SIZE;
        super_block.fs_size = NUM_BLOCKS;
        super_block.inode_table_length = INODE_TABLE_LENGTH;
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
        init_disk("emulated_disk", BLOCK_SIZE, NUM_BLOCKS);

        // read super block
        void *super_block_buff = (void*) malloc(BLOCK_SIZE); // 
        read_blocks(0, 1, super_block_buff);
        super_block = *((SUPER_BLOCK*) super_block_buff);
        free(super_block_buff);

        // validate super block
        if (super_block.magic_number != 0xABCD0005)
        {
            printf("error reading magic number in super block\n");
            exit(1);
        }
        if (super_block.block_size != BLOCK_SIZE)
        {
            printf("super block has wrong block size\n");
            exit(1);
        }
        if (super_block.fs_size != NUM_BLOCKS)
        {
            printf("super block has wrong file system size\n");
            exit(1);
        }
        if (super_block.inode_table_length != INODE_TABLE_LENGTH)
        {
            printf("super block has wrong inode table length\n");
            exit(1);
        }
        if (super_block.root_dir_inode_num != ROOT_DIR_INODE_NUM)
        {
            printf("super block has wrong root directory inode number\n");
            exit(1);
        }
    }

    // cache inode table
    read_blocks(1, 64, inode_table_cache);

    // get root inode
    root_dir_inode = inode_table_cache[super_block.root_dir_inode_num];

    // cache root directory

    // init open file descriptor table
    init_open_file_descriptor_table();
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
    // create new file if it doesn't exist
    if (rdc_get_inode_num(name) < 0)
    {
        // check if need to allocate new block in dir table
        INODE root_inode = inode_table_cache[ROOT_DIR_INODE_NUM];
        int bytes_left_in_block = BLOCK_SIZE - (root_inode.size % BLOCK_SIZE);
        if (bytes_left_in_block < sizeof(DIR_ENTRY))
        {
            // check if there's enough disk space to allocate a new block
        }
    }

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


/***************** utility functions *****************/

// returns 1 on success
int fm_is_available(int blocks_requested)
{
    int freemap[BLOCK_SIZE / 4];
    int free_blocks_found = 0;

    // read free map
    read_blocks(1 + INODE_TABLE_LENGTH, 1, freemap);

    // query each index of the freemap until enough free blocks are found
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        // test the appropriate bit in the freemap
        if (freemap[i/32] & (1 << (i%32)))
            free_blocks_found++;
        // stop looking for free blocks when enough have been found
        if (free_blocks_found == blocks_requested)
            break;
    }

    return free_blocks_found == blocks_requested;
}

// return -1 if no space
int fm_get_next_address_and_allocate()
{
    int next_address = -1;
    int freemap[BLOCK_SIZE / 4];

    // read free map
    read_blocks(1 + INODE_TABLE_LENGTH, 1, freemap);

    // query each index of the freemap until a free block is found
    int i;
    for (i = 0; i < BLOCK_SIZE; i++)
    {
        // test the appropriate bit in the freemap
        if (freemap[i/32] & (1 << (i%32)))
        {
            // set the ith bit in the freemap and write it to the disk
            freemap[i/32] = freemap[i/32] | (1 << i%32);
            write_blocks(1 + INODE_TABLE_LENGTH, 1, freemap);

            // need to add the offset to the address of the first data block
            next_address = 1 + INODE_TABLE_LENGTH + 1 + i;
            break; // stop searching when a free block is found
        }
    }

    return next_address;
}

void init_open_file_descriptor_table()
{
    // make sure every entry is set to invalid
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        open_file_descriptor_table[i].valid = 0;
}

int get_next_fd()
{
    int next_fd = -1;

    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (!open_file_descriptor_table[i].valid)
        {
            next_fd = i;
            break; // stop searching when the first available entry is found
        }
    }

    return next_fd;
}
