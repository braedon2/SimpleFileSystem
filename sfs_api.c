#include "sfs_api.h"
#include "disk_emu.h"
#include "root_dir_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 8258 // 1 super block
                        // 64 blocks for inode table
                        // 8192 data blocks
                        // 1 free bitmap block
#define INODE_TABLE_LENGTH 64 // in blocks
#define ROOT_DIR_INODE_NUM 0
#define MAX_OPEN_FILES 100


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

/* prototypes */
int fm_is_available(int blocks_requested);
int fm_get_next_address_and_allocate();
void init_open_file_descriptor_table();
int get_next_fd();
int allocate_blocks_to_inode(INODE *inode, int num_blocks);
int inode_index_to_address(INODE inode, int index);
int is_file_open(char *file);

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
    rdc_init();
    if (!fresh && rdc_size() == 0)
    {
        int cur_index = 0;
        int cur_address = inode_index_to_address(root_dir_inode, cur_index);
        DIR_ENTRY block_buf[32];
        read_blocks(cur_address, 1, block_buf);
        for (int i = 0; i < root_dir_inode.size / 32; i++)
        {
            if (i % 32 == 0 && i != 0)
            {
                cur_index++;
                cur_address = inode_index_to_address(root_dir_inode, cur_index);
                read_blocks(cur_address, 1, block_buf);
            }
            rdc_insert(block_buf[i % 32]);
        }
    }

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

// returns -1 on failure
int sfs_fopen(char *name)
{
    if (strlen(name) > MAX_FILENAME)
    {
        return -2;
    }

    int inode_num = rdc_get_inode_num(name);

    // check that file isn't already open
    if (is_file_open(name))
    {
        return -1;
    }

    // create new file if it doesn't exist
    if (inode_num < 0)
    {
        INODE root_inode_buf = inode_table_cache[ROOT_DIR_INODE_NUM];

        // check if need to allocate new block in dir table
        if ((root_inode_buf.size % BLOCK_SIZE) == 0)
        {
            // allocate a block to the root inode and make sure there was
            // enough space to do so
            if (!allocate_blocks_to_inode(&root_inode_buf, 1))
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

        /** write dir entry to dir table on disk **/
        DIR_ENTRY dir_table_section[32]; // one disk block of the dir table
        int dir_table_index = root_inode_buf.size / sizeof(DIR_ENTRY);
        int block_index = dir_table_index / 32; // block to get from inode
                                                // 32 dir entries per block
        int block_address = inode_index_to_address(root_inode_buf, block_index);

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
    int retval = 0;

    if (!open_file_descriptor_table[fileID].valid)
    {
        printf("attempt to close unopened file\n");
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
        printf("file id %d does not refer to an open file\n", fileID);
        return 0;
    }

    INODE inode = inode_table_cache[open_file_descriptor_table[fileID].inode_num];

    if (loc > inode.size)
    {
        printf("attempt to seek out of bounds\n");
        return 0;
    }

    open_file_descriptor_table[fileID].rptr = loc;
    return 1;
}

int sfs_fwseek(int fileID, int loc)
{
    if (!open_file_descriptor_table[fileID].valid)
    {
        printf("file id %d does not refer to an open file\n", fileID);
        return 0;
    }

    INODE inode = inode_table_cache[open_file_descriptor_table[fileID].inode_num];

    if (loc > inode.size)
    {
        printf("attempt to seek out of bounds\n");
        return 0;
    }

    open_file_descriptor_table[fileID].wptr = loc;
    return 1;
}

// treats inode like 2d array
// ith byte in inode = inode[wptr / BLOCK_SIZE][wptr % BLOCK_SIZE]
// returns the amount of bytes written
int sfs_fwrite(int fileID, char *buf, int length)
{
    int bytes_written = 0;
    OPEN_FILE_DESCRIPTOR_TABLE_ENTRY* fde_ptr = &(open_file_descriptor_table[fileID]);
    INODE *inode_ptr = &(inode_table_cache[fde_ptr->inode_num]); 

    if (!fde_ptr->valid)
    {
        return bytes_written;
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
            if (!allocate_blocks_to_inode(inode_ptr, 1))
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
        return bytes_read;
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
    int freemap[BLOCK_SIZE / 4];
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

    // remove dir entry from cache
    rdc_remove(file);

    // see if a block can be deallocated from root dir inode

    // write cache to disk
    int i = 0;
    INODE *root_inode_ptr = &(inode_table_cache[ROOT_DIR_INODE_NUM]);
    DIR_ENTRY block_buf[32];
    memset(block_buf, 0, sizeof(block_buf));
    RDC_NODE *cur_node = rdc_head();
    int cur_inode_i = 0;
    int cur_address = inode_index_to_address(*root_inode_ptr, cur_inode_i);
    while (cur_node != NULL)
    {
        if (i % 32 == 0 && i != 0)
        {
            write_blocks(cur_address, 1, block_buf);
            cur_inode_i++;
            cur_address = inode_index_to_address(*root_inode_ptr, cur_inode_i);
            memset(block_buf, 0, sizeof(block_buf));
        }
        block_buf[i] = cur_node->data;
        cur_node = cur_node->next;
    }
    write_blocks(cur_address, 1, block_buf);
    root_inode_ptr->size -= 32;

    return 0; 
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

// return 0 if no space available
int allocate_blocks_to_inode(INODE *inode, int num_blocks)
{
    int blocks_required = num_blocks;
    // number of blocks currently used by inode
    int current_blocks = inode->size / BLOCK_SIZE; // TODO this is a bug!!!

    // check that the number of allocated blocks won't exceed what an inode
    // can refer to
    if (current_blocks + num_blocks > (12 + 256))
        return 0;

    // account for if we need to allocate the indirect pointer block
    if ((current_blocks <= 12) && (current_blocks + num_blocks > 12))
        blocks_required++;

    // check that the requested number of blocks are available
    if (!fm_is_available(num_blocks))
        return 0;

    for (int i = 0; i < num_blocks; i++)
    {
        int inode_block_pointer_index = (inode->size / BLOCK_SIZE) + i;

        // allocate new block
        int new_block_address = fm_get_next_address_and_allocate();

        // allocate indirect pointer block if necessary
        if (inode_block_pointer_index == 12)
        {
            inode->ind_ptr = fm_get_next_address_and_allocate();
        }
        
        // set new block pointer in inode
        if (inode_block_pointer_index < 12)
        {
            inode->direct_ptr[inode_block_pointer_index] = new_block_address;
        }
        else if (inode_block_pointer_index >= 12) // set in indirect pointer table
        {
            // read indirect block
            int indirect_block_buf[256]; // holds 256 direct addresses
            read_blocks(inode->ind_ptr, 1, indirect_block_buf);
            
            // set direct pointer in indirect block and write it to disk
            indirect_block_buf[inode_block_pointer_index - 12] = new_block_address;
            write_blocks(inode->ind_ptr, 1, indirect_block_buf);
        }
    }

    return 1;
}

int inode_index_to_address(INODE inode, int index)
{
    if (index > (256 + 12))
        return -1;

    int block_address;
    // get block address from inode
    if (index < 12) // direct pointer
    {
        block_address = inode.direct_ptr[index];
    }
    else
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