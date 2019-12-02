#include "root_dir_cache.h"
#include <stdlib.h>
#include <string.h>



int size = 0;
RDC_NODE *head = NULL;
RDC_NODE *tail = NULL;
RDC_NODE *cur_listing = NULL;

void rdc_init()
{
    cur_listing = head;
}

RDC_NODE *rdc_head()
{
    return head;
}

int rdc_size()
{
    return size;
}

// TODO check for duplicates
int rdc_insert(DIR_ENTRY dir_entry)
{
    RDC_NODE *new_node = (RDC_NODE*) malloc(sizeof(RDC_NODE));

    // return immediately if allocation fails
    if (new_node == NULL)
    {
        return 1;
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
        return 1;
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