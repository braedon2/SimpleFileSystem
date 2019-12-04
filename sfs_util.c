#include "common.h"
#include "sfs_util.h"
#include "root_dir_cache.h"
#include "disk_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// returns 1 on success
int fm_is_available(int blocks_requested)
{
    // BLOCK_SIZE * 8 is the number of bits needed
    // dividing by 32 gives the number of ints needed
    int freemap[(BLOCK_SIZE * 8) / 32]; 
    int free_blocks_found = 0;

    // read free map
    read_blocks(1 + INODE_TABLE_LENGTH, 1, freemap);

    // query each index of the freemap until enough free blocks are found
    int i;
    for (i = 0; i < BLOCK_SIZE; i++)
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
    int freemap[(BLOCK_SIZE * 8) / 32];

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

// return 0 if no space available
int allocate_block_to_inode(INODE *inode)
{
    

    // number of blocks currently used by inode
    int current_blocks = (inode->size == 0) ? 0 : inode->size / BLOCK_SIZE + 1;
    // need to allocate two blocks if we need to allocate the indirect pointer block
    int blocks_required = (current_blocks == 12) ? 2 : 1;

    // check that allocating a block block won't exceed what an inode
    // can refer to
    if (current_blocks + 1 > (12 + 256))
        return -1;

    // check that the requested number of blocks are available
    if (!fm_is_available(blocks_required))
        return -1;

    // allocate new block
    int new_block_address = fm_get_next_address_and_allocate();

    // allocate indirect pointer block if necessary
    if (current_blocks == 12)
    {
        inode->ind_ptr = fm_get_next_address_and_allocate();
    }
    
    // set new block pointer in inode
    if (current_blocks < 12)
    {
        inode->direct_ptr[current_blocks] = new_block_address;
    }
    else // set in indirect pointer table
    {
        // read indirect block
        int indirect_block_buf[256]; // holds 256 direct addresses
        read_blocks(inode->ind_ptr, 1, indirect_block_buf);
        
        // set direct pointer in indirect block and write it to disk
        indirect_block_buf[current_blocks - 12] = new_block_address;
        write_blocks(inode->ind_ptr, 1, indirect_block_buf);
    }
    
    return 0;
}

int inode_index_to_address(INODE inode, int index)
{
    // check if the requested index is larger than the number of blocks allocated
    if (index > (inode.size / BLOCK_SIZE))
        return -1;

    int block_address;
    // get block address from inode
    if (index < 12) // get address from direct pointer
    {
        block_address = inode.direct_ptr[index];
    }
    else // get address from indirect pointer
    {
        // read indirect block
        int indirect_block[256];
        read_blocks(inode.ind_ptr, 1, indirect_block);
        block_address = indirect_block[index - 12];
    }

    return block_address;
}

// return 1 if file already open
int is_file_open(char *file)
{
    int file_already_open = 0;
    int inode_num = rdc_get_inode_num(file);

    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        OPEN_FILE_DESCRIPTOR_TABLE_ENTRY table_entry = open_file_descriptor_table[i];
        if (table_entry.valid && table_entry.inode_num == inode_num)
            file_already_open = 1;
    }
    return file_already_open;
}