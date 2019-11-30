#include "sfs_api.h"
#include "disk_emu.h"
#include "root_dir_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 8258 // 1 super block
                        // 64 blocks for inode table
                        // 8192 data blocks
                        // 1 free bitmap block
#define INODE_TABLE_LENGTH 64 // in blocks
#define ROOT_DIR_INODE_NUM 0
#define MAX_OPEN_FILES 100

int fm_is_available(int blocks_requested);
void init_open_file_descriptor_table();
int fm_get_next_address_and_allocate();
int get_next_fd();

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
    int ind_ptr; // table of 256 direct pointers
    int : 32; // int comes first for proper alignment
    long long : 64;
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
    printf("size of inode: %ld\n", sizeof(INODE));

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

        // initialize inode table cache
        for (int i = 0; i < 512; i++)
        {
            inode_table_cache[i].valid = 0;
        }
        // write root directory to first entry of inode table cache
        root_dir_inode.valid = 1;
        root_dir_inode.mode = 0;
        root_dir_inode.link_count = 1;
        root_dir_inode.uid = 0;
        root_dir_inode.gid = 0;
        root_dir_inode.size = 0;
        inode_table_cache[ROOT_DIR_INODE_NUM] = root_dir_inode;
        // write inode table cache to disk
        write_blocks(1, INODE_TABLE_LENGTH, inode_table_cache);
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
    read_blocks(1, INODE_TABLE_LENGTH, inode_table_cache);

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

// returns -1 on failure
int sfs_fopen(char *name)
{
    int inode_num = rdc_get_inode_num(name);

    // create new file if it doesn't exist
    if (inode_num < 0)
    {
        INODE root_inode_buf = inode_table_cache[ROOT_DIR_INODE_NUM];

        // check if need to allocate new block in dir table
        if ((root_inode_buf.size % BLOCK_SIZE) == 0)
        {
            // will need two blocks if the indirect pointer isn't set
            int blocks_required = (root_inode_buf.size / BLOCK_SIZE == 12) ? 2 : 1;
            // check if there's enough disk space to allocate new blocks
            if (fm_is_available(blocks_required))
            {
                int inode_block_pointer_index = root_inode_buf.size / BLOCK_SIZE;

                // allocate new block
                int new_block_address = fm_get_next_address_and_allocate();

                // allocate indirect pointer block if necessary
                if (inode_block_pointer_index == 12)
                {
                    root_inode_buf.ind_ptr = fm_get_next_address_and_allocate();
                }
                
                // set new block pointer in root inode
                if (inode_block_pointer_index < 12)
                {
                    root_inode_buf.direct_ptr[inode_block_pointer_index] = new_block_address;
                }
                else if (inode_block_pointer_index >= 12) // set in indirect pointer table
                {
                    // read indirect block
                    int indirect_block_buf[256]; // holds 256 direct addresses
                    read_blocks(root_inode_buf.ind_ptr, 1, indirect_block_buf);
                    
                    // set direct pointer in indirect block and write it to disk
                    indirect_block_buf[inode_block_pointer_index - 12] = new_block_address;
                    write_blocks(root_inode_buf.ind_ptr, 1, indirect_block_buf);
                }
            }
            else
            {
                printf("insufficient space to open file\n");
                return -1;
            }
        }

        // allocate inode for file and write to disk
        for (int i = 0; i < INODE_TABLE_LENGTH; i++)
        {
            if (!inode_table_cache[i].valid)
            {
                inode_num = i;
                break;
            }
        }
        if (inode_num < 0)
        {
            printf("insufficient inodes\n");
            return -1;
        }
        inode_table_cache[inode_num].valid = 1;
        inode_table_cache[inode_num].size = 0; // disk is updated later

        // write dir entry to root directory cache
        DIR_ENTRY dir_entry;
        dir_entry.inode_num = inode_num;
        strcpy(dir_entry.filename, name);
        rdc_insert(dir_entry);

        /** write dir entry to dir table on disk **/
        DIR_ENTRY dir_table_section[32]; // one disk block of the dir table
        int dir_table_index = root_inode_buf.size / sizeof(DIR_ENTRY);
        int block_index = dir_table_index / 32; // block to get from inode
                                                // 32 dir entries per block
        int block_address;
        // get block address from inode
        if (block_index < 12) // direct pointer
        {
            block_address = root_inode_buf.direct_ptr[block_index];
        }
        else
        {
            // read indirect block
            int indirect_block[256];
            read_blocks(root_inode_buf.ind_ptr, 1, indirect_block);
            block_address = indirect_block[block_index - 12];
        }
        // read dir table section to be written to
        read_blocks(block_address, 1, dir_table_section);
        // set dir table entry to the newly created directory entry
        dir_table_section[dir_table_index % 32] = dir_entry;
        // write the updated section back to the disk
        write_blocks(block_address, 1, dir_table_section);

        // update size of root inode
        root_inode_buf.size += sizeof(DIR_ENTRY);
        // update root inode cache
        inode_table_cache[ROOT_DIR_INODE_NUM] = root_inode_buf;
        // update inode table in disk
        write_blocks(1, INODE_TABLE_LENGTH, inode_table_cache);
    }

    int fd = get_next_fd();
    if (fd < 0)
    {
        printf("error: maximum number of files already open\n");
        return -1;
    }

    open_file_descriptor_table[fd].valid = 1;
    open_file_descriptor_table[fd].rptr = 0;
    open_file_descriptor_table[fd].wptr = inode_table_cache[inode_num].size;
    open_file_descriptor_table[fd].inode_num = inode_num;

    return fd;
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
        if ((freemap[i/32] & (1 << (i%32))) == 0)
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
        if ((freemap[i/32] & (1 << (i%32))) == 0)
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
