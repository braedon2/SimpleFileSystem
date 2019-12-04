#include "common.h"

/**
 * returns 1 if the given number of blocks is available in the free map.
 * returns 0 otherwise
 */
int fm_is_available(int blocks_requested);

/**
 * sets the next free bit in the free map to allocated and returns the address
 * of its associated block
 * returns -1 if no blocks are free, i.e. the disk is full
 */
int fm_get_next_address_and_allocate();

/**
 * sets every entry in the open file descriptor table to invalid
 */
void init_open_file_descriptor_table();

/**
 * returns the index of the first invalid file descriptor entry.
 * returns -1 if no entry is found, i.e. the maximum number of files has 
 * already been opened
 */
int get_next_fd();

/**
 * allocates a block and sets the given inode to point to it. the function uses
 * the inodes size to determine what pointer to set so it should only be called 
 * when no more data can be written with the current number of allocated blocks
 * 
 * returns -1 if the allocation fails
 * returns 0 on success
 */
int allocate_block_to_inode(INODE *inode);

/**
 * maps an inodes block pointer to its associated disk-address. 
 * 
 * returns -1 if the index refers to a pointer that does not point to an 
 * allocated block
 * returns 0 on success
 */
int inode_index_to_address(INODE inode, int index);
int is_file_open(char *file);