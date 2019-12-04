#include "common.h"

/**
 * initializes the cache with the contents of the on-disk root directory
 */
void rdc_init();

/**
 * writes the cache to the on-disk root directory. this function does not
 * allocate blocks to the root directory inode so this must be taken care of
 * beforehand
 * 
 * returns 0 on success
 * returns -1 if the number of blocks required to write the cache to disk is
 * larger than the number of blocks allocated to the root directory inode.
 */
int rdc_to_disk();

/**
 * returns the number of entries in the cache
 */
int rdc_size();

/**
 * inserts the given entry to the end of the cache
 * 
 * returns -1 if the function failed to allocate memory for the new entry.
 * returns 0 on success
 */
int rdc_insert(DIR_ENTRY dir_entry);

/**
 * removes an entry from the cache based on its filename
 * 
 * returns -1 if the entry is not found
 * returns 0 on success
 */
int rdc_remove(char *filename);

/**
 * finds the directory entry that matches the filename and returns
 * the associated inode number
 * 
 * returns -1 if no entry is found
 */
int rdc_get_inode_num(const char *filename);

/**
 * keeps track of the current file in the list and stores the next file in 
 * the listing in filename
 * 
 * any modification to the underlying list (inserting, removing) will restart
 * the listing from the beginning
 * 
 * returns 0 if the end of the list is reached at which point the point will
 * restart the listing at the beginning
 */
int rdc_getnextfilename(char *filename);