#include "common.h"
#include "sfs_api.h"
#include "disk_emu.h"
#include "root_dir_cache.h"
#include "sfs_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
    rdc_init(root_dir_inode);

    // init open file descriptor table
    init_open_file_descriptor_table();
}

// return 1 on success
int sfs_getnextfilename(char *fname)
{
    return rdc_getnextfilename(fname);
}

// return -1 if no file
int sfs_getfilesize(const char* path)
{
    int inode_num = rdc_get_inode_num(path);

    if (inode_num == -1)
        return -1;

    return inode_table_cache[inode_num].size;
}


int sfs_fopen(char *name)
{
    if (strlen(name) > MAX_FILENAME)
    {
        return -1;
    }

    // check that file isn't already open
    if (is_file_open(name))
    {
        return -1;
    }

    int inode_num = rdc_get_inode_num(name);

    // create new file if it doesn't exist
    if (inode_num < 0)
    {
        INODE *root_inode_ptr = &(inode_table_cache[ROOT_DIR_INODE_NUM]);

        // check if need to allocate new block in dir table
        if ((root_inode_ptr->size % BLOCK_SIZE) == 0)
        {
            // allocate a block to the root inode and make sure there was
            // enough space to do so
            if (!allocate_blocks_to_inode(root_inode_ptr, 1))
            {
                printf("insufficient space to create file\n");
                return -1;
            }
        }

        // allocate inode for file and write to disk
        for (int i = 0; i < INODE_TABLE_LENGTH * (BLOCK_SIZE/sizeof(INODE)); i++)
        {
            if (!inode_table_cache[i].valid)
            {
                inode_num = i;
                break;
            }
        }
        if (inode_num < 0)
        {
            printf("insufficient inodes to create file\n");
            return -1;
        }
        inode_table_cache[inode_num].valid = 1;
        inode_table_cache[inode_num].size = 0; // disk is updated later

        // write dir entry to root directory cache
        DIR_ENTRY dir_entry;
        dir_entry.inode_num = inode_num;
        strcpy(dir_entry.filename, name);
        rdc_insert(dir_entry);

        // write dir entry to dir table on disk
        rdc_to_disk();
        // DIR_ENTRY dir_table_section[32]; // one disk block of the dir table
        // int dir_table_index = root_inode_ptr->size / sizeof(DIR_ENTRY);
        // int block_index = dir_table_index / 32; // block to get from inode
        //                                         // 32 dir entries per block
        // int block_address = inode_index_to_address(*root_inode_ptr, block_index);

        // // read dir table section to be written to
        // read_blocks(block_address, 1, dir_table_section);
        // // set dir table entry to the newly created directory entry
        // dir_table_section[dir_table_index % 32] = dir_entry;
        // // write the updated section back to the disk
        // write_blocks(block_address, 1, dir_table_section);

        // update size of root inode
        root_inode_ptr->size += sizeof(DIR_ENTRY);
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
    int retval = 0;

    if (!open_file_descriptor_table[fileID].valid)
    {
        retval = 1;
    }
    else
    {
        open_file_descriptor_table[fileID].valid = 0;
    }
    return retval;
}

int sfs_frseek(int fileID, int loc)
{
    if (!open_file_descriptor_table[fileID].valid)
    {
        return -1;
    }

    INODE inode = inode_table_cache[open_file_descriptor_table[fileID].inode_num];

    if (loc > inode.size)
    {
        printf("attempt to seek out of bounds\n");
        return -1;
    }

    open_file_descriptor_table[fileID].rptr = loc;
    return 0;
}

int sfs_fwseek(int fileID, int loc)
{
    if (!open_file_descriptor_table[fileID].valid)
    {
        printf("file id %d does not refer to an open file\n", fileID);
        return -1;
    }

    INODE inode = inode_table_cache[open_file_descriptor_table[fileID].inode_num];

    if (loc > inode.size)
    {
        printf("attempt to seek out of bounds\n");
        return -1;
    }

    open_file_descriptor_table[fileID].wptr = loc;
    return 0;
}

// treats inode like 2d array
// ith byte in inode = inode[wptr / BLOCK_SIZE][wptr % BLOCK_SIZE]
// returns the amount of bytes written
int sfs_fwrite(int fileID, const char *buf, int length)
{
    int bytes_written = 0;
    OPEN_FILE_DESCRIPTOR_TABLE_ENTRY* fde_ptr = &(open_file_descriptor_table[fileID]);
    INODE *inode_ptr = &(inode_table_cache[fde_ptr->inode_num]); 

    if (!fde_ptr->valid)
    {
        return -1;
    }

    int cur_inode_i = fde_ptr->wptr / BLOCK_SIZE;
    int cur_block_addr;
    char *block_buf = (char*) malloc(BLOCK_SIZE);

    for (int i = 0; i < length; i++)
    {
        // need to allocate new block
        if (fde_ptr->wptr % BLOCK_SIZE == 0 && fde_ptr->wptr == inode_ptr->size)
        {
            // stop writing if there's no space left on disk
            if (!allocate_block_to_inode(inode_ptr, 1))
                break;
        }

        // need to get next block
        if (fde_ptr->wptr % BLOCK_SIZE == 0 || i == 0)
        {
            // get address here to ensure it has been allocated in inode first
            cur_block_addr = inode_index_to_address(*inode_ptr, cur_inode_i);

            // write prev block to disk
            if (i != 0)
            {
                write_blocks(cur_block_addr, 1, block_buf);
                cur_inode_i++;
                cur_block_addr = inode_index_to_address(*inode_ptr, cur_inode_i);
            }
            // get next block
            read_blocks(cur_block_addr, 1, block_buf);
        }

        block_buf[fde_ptr->wptr % BLOCK_SIZE] = buf[i];
        fde_ptr->wptr++;
        bytes_written++;
        
        if ((fde_ptr->wptr - 1) == inode_ptr->size)
        {
            inode_ptr->size++;
        }
    }

    write_blocks(cur_block_addr, 1, block_buf);
    free(block_buf);

    // update cache to disk
    write_blocks(1, INODE_TABLE_LENGTH, inode_table_cache);
    return bytes_written;
}

int sfs_fread(int fileID, char *buf, int length)
{
    int bytes_read = 0;
    OPEN_FILE_DESCRIPTOR_TABLE_ENTRY* fde_ptr = &(open_file_descriptor_table[fileID]);
    INODE *inode_ptr = &(inode_table_cache[fde_ptr->inode_num]);

    if (!fde_ptr->valid)
    {
        return -1;
    }

    int cur_inode_i = fde_ptr->rptr / BLOCK_SIZE;
    int cur_inode_addr = inode_index_to_address(*inode_ptr, cur_inode_i);
    char *block_buf = (char*) malloc(BLOCK_SIZE);

    for (int i = 0; i < length; i++)
    {
        // don't try and read past the size of the file
        if (fde_ptr->rptr == inode_ptr->size)
            break;

        // need to get next block
        if (fde_ptr->rptr % BLOCK_SIZE == 0 || i == 0)
        {
            if (i != 0)
            {
                cur_inode_i++;
                cur_inode_addr = inode_index_to_address(*inode_ptr, cur_inode_i);
            }
            // get next block
            read_blocks(cur_inode_addr, 1, block_buf);
        }

        buf[i] = block_buf[fde_ptr->rptr % BLOCK_SIZE];
        fde_ptr->rptr++;
        bytes_read++;
    }

    free(block_buf);
    return bytes_read;
}

// return zero on success
int sfs_remove(char *file)
{
    // don't remove if file is open
    if (is_file_open(file))
    {
        return 1;
    }

    int inode_num = rdc_get_inode_num(file);
    if (inode_num == -1)
    {
        return 1;
    }

    INODE *inode_ptr = &(inode_table_cache[inode_num]);
    int freemap[BLOCK_SIZE * 8 / 32];
    read_blocks(1 + INODE_TABLE_LENGTH, 1, freemap);

    int blocks_to_free = inode_ptr->size / BLOCK_SIZE;
    if (inode_ptr->size % BLOCK_SIZE != 0) blocks_to_free++;

    int addr_to_free;
    int b; // bit to free

    // set data blocks pointed by inode to free in freemap
    for (int i = 0; i < blocks_to_free; i++)
    {
        addr_to_free = inode_index_to_address(*inode_ptr, i);
        b = addr_to_free - 1 - INODE_TABLE_LENGTH; // bit to free
        freemap[b/32] = freemap[b/32] & ~(1 << b%32); // clear bit
    }

    // set indirect pointer to free if necessary
    if (blocks_to_free >= 12)
    {
        addr_to_free = inode_ptr->ind_ptr;
        b = addr_to_free - 1 - INODE_TABLE_LENGTH;
        freemap[b/32] = freemap[b/32] & ~(1 << b%32); // clear bit
    }

    // set inode entry to invalid
    inode_ptr->valid = 0;

    // remove dir entry from cache and update disk
    rdc_remove(file);
    rdc_to_disk();
    inode_table_cache[ROOT_DIR_INODE_NUM].size -= sizeof(DIR_ENTRY);
    write_blocks(1, INODE_TABLE_LENGTH, inode_table_cache);

    return 0; 
}