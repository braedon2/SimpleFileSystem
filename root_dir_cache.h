
typedef struct DIR_ENTRY{
    char *filename;
    int inode_num;
} DIR_ENTRY;

int rdc_insert(DIR_ENTRY dir_entry);
int rdc_remove(char *filename);
int rdc_get_inode_num(char *filename);