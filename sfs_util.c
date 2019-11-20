#include "sfs_util.h"

void formatFreshDisk()
{
    int block_size = 1024;
    int fs_size = 8388674;
    int inode_table_length = 64;

    // init with 8 megabytes of free space
    init_fresh_disk("emulated_disk", block_size, fs_size);

    // write super block to first block of disk
    SUPER_BLOCK super_block = {
        .magic_number = 0xABCD0005,
        .block_size = block_size,
        .fs_size = fs_size,
        .inode_table_length = inode_table_length,
        .root_dir_inode_num = 0
    };
    write_blocks(0, 1, &super_block);

    
}