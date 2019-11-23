#include "root_dir_cache.h"
#include <stdlib.h>
#include <string.h>

typedef struct RDC_NODE {
    DIR_ENTRY data;
    struct RDC_NODE *next;
} RDC_NODE;

RDC_NODE *head = NULL;
RDC_NODE *tail = NULL;

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
    
    return 0;
}

int rdc_remove(DIR_ENTRY dir_entry)
{
    RDC_NODE *prev_node = NULL;
    RDC_NODE *cur_node = head;
    int found = 0;

    // search for dir entry
    while (cur_node != NULL && !found)
    {
        if (strcmp(cur_node->data.filename, dir_entry.filename) &&
            cur_node->data.inode_num == dir_entry.inode_num)
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
    
    free(cur_node);
    return 0;
}