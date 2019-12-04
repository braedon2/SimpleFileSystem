
/**
 * creates the file system
 * 
 * fresh - 1 if the file system does not exist already
 */
void mksfs(int fresh); // creates the file system

/**
 * stores the next filename of the listing in fname
 * 
 * returns 1 if there was a next filename, otherwise 0
 */
int sfs_getnextfilename(char *fname); 

/**
 * returns the size of the given file in bytes
 * returns -1 if the file does not exist
 */
int sfs_getfilesize(const char* path); // get the size of the given file

/**
 * opens the given file for reading and writing. creates the file if it does
 * not exist already
 * 
 * returns the file ID on success, returns -1 otherwise
 */ 
int sfs_fopen(char *name); 

/**
 * closes the file, i.e. removes it from the open file descriptor table
 * 
 * return 0 if the file was succesfully closed. 1 if the file was not open
 * in the first place
 */
int sfs_fclose(int fileID);

/**
 * moves the open file's read pointer to the given location from the beginning
 * 
 * returns 0 if the seek was succesful. -1 if the file doesn't exist or the 
 * request is larger than the file's size
 */
int sfs_frseek(int fileID, int loc);

/**
 * moves the open file's write pointer to the given location from the beginning
 * 
 * returns - if the seek was succesful. -1 if the file doesn't exist or the request
 * is larger than the file's size
 */
int sfs_fwseek(int fileID, int loc);

/**
 * writes characters to the disk starting from the file descriptors write
 * pointer
 * 
 * fileID - index in the open file descriptor table for the open file
 * buf - pointer to the data to be written
 * length - the number of bytes to write
 * 
 * returns the number of bytes written. returns -1 if the file id does not
 * refer to an open file
 */
int sfs_fwrite(int fileID, const char *buf, int length); 

/**
 * reads characters from the disk starting from the file descriptors read
 * pointer
 * 
 * fileID - index in the open file desriptor table for the open file
 * buf - pointer to a buffer to store the read data
 * length - number of bytes to read from the disk
 * 
 * returns the number of bytes read. returns -1 if the file id does not
 * refer to an open file
 */
int sfs_fread(int fileID, char *buf, int length);

/**
 * removes a file from the file system and deallocates its data blocks
 * 
 * returns -1 if the file does not exist
 * returns 0 on success
 */
int sfs_remove(char *file); // removes a file from the filesystem