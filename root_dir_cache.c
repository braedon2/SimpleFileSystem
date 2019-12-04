#include "root_dir_cache.h"
#include "sfs_util.h"
#include "disk_emu.h"
#include <stdlib.h>
#include <string.h>

typedef struct RDC_NODE {
    DIR_ENTRY data;
    struct RDC_NODE *next;
} RDC_NODE;

int size = 0;
RDC_NODE *head = NULL;
RDC_NODE *tail = NULL;
RDC_NODE *cur_listing = NULL;

void rdc_init()
{
    INODE *root_inode = &(inode_table_cache[ROOT_DIR_INODE_NUM]);

    // erase contents
    RDC_NODE *cur_node = head;
    while (cur_node != NULL)
    {
        RDC_NODE *to_free = cur_node;
        cur_node = cur_node->next;
        free(to_free);
    }
    head = NULL;
    tail = NULL;

    // initalize the list with the table pointed to by the given inode
    int cur_index = 0;
    int cur_address = inode_index_to_address(*root_inode, cur_index);
    DIR_ENTRY block_buf[32];
    read_blocks(cur_address, 1, block_buf);
    for (int i = 0; i < root_inode->size / 32; i++)
    {
        if (i % 32 == 0 && i != 0)
        {
            cur_index++;
            cur_address = inode_index_to_address(*root_inode, cur_index);
            read_blocks(cur_address, 1, block_buf);
        }
        rdc_insert(block_buf[i % 32]);
    }

    // start listing at the beginning of the list
    cur_listing = head;
}

int rdc_to_disk()
{
    INODE *root_inode_ptr = &(inode_table_cache[ROOT_DIR_INODE_NUM]);

    // write cache to disk
    int i = 0;
    DIR_ENTRY block_buf[32];
    memset(block_buf, 0, sizeof(block_buf));
    RDC_NODE *cur_node = head;
    int cur_inode_i = 0;
    int cur_address = inode_index_to_address(*root_inode_ptr, cur_inode_i);
    while (cur_node != NULL && i != size)
    {
        if (cur_address == -1)
        {
            return -1;
        }

        // write the buffer to the disk when it is full
        if (i % 32 == 0 && i != 0)
        {
            write_blocks(cur_address, 1, block_buf);
            cur_inode_i++;
            cur_address = inode_index_to_address(*root_inode_ptr, cur_inode_i);
            memset(block_buf, 0, sizeof(block_buf)); // reset the buffer
        }
        block_buf[i % 32] = cur_node->data;
        cur_node = cur_node->next;
        i++;
    }
    write_blocks(cur_address, 1, block_buf);

    return 0;
}


int rdc_size()
{
    return size;
}


int rdc_insert(DIR_ENTRY dir_entry)
{
    RDC_NODE *new_node = (RDC_NODE*) malloc(sizeof(RDC_NODE));

    // return immediately if allocation fails
    if (new_node == NULL)
    {
        return -1;
    }

    new_node->data = dir_entry;
    new_node->next = NULL;

    // set up the head and tail pointers differently when this is the first
    // entry
    if (head == NULL)
    {
        head = new_node;
        tail = new_node;
    }
    else
    {
        tail->next = new_node;
        tail = new_node;
    }

    cur_listing = head; // restart listing
    size++;
    return 0;
}

int rdc_remove(char *filename)
{
    RDC_NODE *prev_node = NULL;
    RDC_NODE *cur_node = head;
    int found = 0;

    // search for dir entry
    while (cur_node != NULL && !found)
    {
        if (strcmp(cur_node->data.filename, filename) == 0)
        {
            found = 1;
        }
        else // want to maintain the node pointer position when the entry is found
        {
            prev_node = cur_node;
            cur_node = cur_node->next;
        }
    }

    // return with failure if entry not found
    if (!found)
    {
        return -1;
    }

    // boundary condition, removing the first node
    if (prev_node == NULL)
    {
        head = head->next;
    }
    // boundary condition, removing the last node
    else if (cur_node->next == NULL)
    {
        prev_node->next = NULL;
    }
    else
    {
        prev_node->next = cur_node->next;
    }

    // removing a file that is being listed will cause the listing to fail
    if (cur_node == cur_listing)
        cur_listing = NULL;
    
    free(cur_node);
    cur_listing = head; // restart listing
    size--;
    return 0;
}

int rdc_get_inode_num(const char *filename)
{
    int inode_num = -1;
    RDC_NODE *cur_node = head;
    
    while (cur_node != NULL)
    {
        if (strcmp(cur_node->data.filename, filename) == 0)
        {
            inode_num = cur_node->data.inode_num;
        }
        cur_node = cur_node->next;
    }
    return inode_num;
}

// return 1 if succesful
int rdc_getnextfilename(char *filename)
{
    if (cur_listing == NULL)
    {
        cur_listing = head; // reset listing
        return 0;
    }

    strcpy(filename, cur_listing->data.filename);
    cur_listing = cur_listing->next;
    return 1;
}