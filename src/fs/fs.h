#ifndef FS_H
#define FS_H

#include <sys/types.h>

/*
 * Structure for our file system:
 * Underlying physical structure: inodes. We are not using a full VFS. We store the hierarchical structure in inodes (as a "parent" field).
 * Opened files and directories are registered in an open file table (details below).
 *
 * Brief procedures for a call to f_open(/usr/include/fs.h, mode):
 *  1. start from the inode for "/" or current working directory
 *  2. locate its data block, search for the directory for "/usr/" and its inode
 *  3. repeat until we find the inode for "fs.h"
 *  4. create an instance of file_t (see struct def below) and add it to the open file table
 *  5. return an fd associated with the file_t instance to the user process
 *
 * File table:
 * Implemented as a linked list.
 * An entry in the file table consists of a file descriptor (int) and a file struct (file_t).
 * A file_t contains an inode number and a file position indicator.
 *
 * Free-space management: a list of free blocks and a list of free inodes
 *
 * Directory file structures:
 * A directory file is an array of directory entries. A directory entry consists of the inode number and the name of the file.
 *
 */

#define BOOT_SIZE 512
#define SUPER_SIZE 512
#define N_DBLOCKS 10
#define MAX_NAME_LEN _

typedef struct {
    int inode;
    long position; // file position indicator
} file_t;

typedef struct {
    int inode;
    char name[MAX_NAME_LEN];
} dir_t;

int f_open(const char* pathname, const char* mode);
ssize_t f_read(int fd, void *buf, size_t count);
ssize_t f_write(int fd, const void *buf, size_t count);
int f_seek(int fd, long offset, int whence);
void f_rewind(int fd);
int f_stat(int fd, inode_t* inode);
int f_remove(int fd);
file_t* f_opendir(const char* pathname);
dir_t f_readdir(int fd);
int f_closedir(int fd);
int f_mkdir(const char* pathname, const char* mode);
int f_rmdir(const char* pathname);
int f_mount(const char* source, const char* target);
int f_umount(const char* target);

#endif
