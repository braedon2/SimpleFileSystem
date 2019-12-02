#define MAX_FILENAME 28

// 32 dir entries per block
typedef struct DIR_ENTRY{
    char filename[MAX_FILENAME];
    int inode_num;
} DIR_ENTRY;

typedef struct RDC_NODE {
    DIR_ENTRY data;
    struct RDC_NODE *next;
} RDC_NODE;

void rdc_init();
RDC_NODE *rdc_head();
int rdc_size();
int rdc_insert(DIR_ENTRY dir_entry);
int rdc_remove(char *filename);
int rdc_get_inode_num(const char *filename);
int rdc_getnextfilename(char *filename);