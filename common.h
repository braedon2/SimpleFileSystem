#ifndef _COMMON_H_
#define _COMMON_H_

#define MAX_FILENAME 28
#define MAX_OPEN_FILES 100

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 8258 // 1 super block
                        // 64 blocks for inode table
                        // 8192 data blocks
                        // 1 free bitmap block
#define INODE_TABLE_LENGTH 64 // in blocks
#define ROOT_DIR_INODE_NUM 0

// struct has a size of 32 bytes so there will be 32 dir entries per block
typedef struct DIR_ENTRY{
    char filename[MAX_FILENAME];
    int inode_num;
} DIR_ENTRY;

typedef struct SUPER_BLOCK {
    int magic_number;
    int block_size;
    int fs_size;
    int inode_table_length;
    int root_dir_inode_num;
} SUPER_BLOCK;

// padded to 128 bytes so exactly 8 inodes fit on a block
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

typedef struct OPEN_FILE_DESCRIPTOR_TABLE_ENTRY {
    int valid; // indicates if the entry refer to an open file
    int inode_num;
    int rptr;
    int wptr;
} OPEN_FILE_DESCRIPTOR_TABLE_ENTRY;

INODE inode_table_cache[512];

OPEN_FILE_DESCRIPTOR_TABLE_ENTRY open_file_descriptor_table[MAX_OPEN_FILES];

#endif
