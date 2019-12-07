#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include "disk.h"

#define MAX_NAME_LEN (BLOCKSIZE / 2 - sizeof(long long) - 1)

typedef struct {
    long long inode;
    long long position; // file position indicator
} file_t;

typedef struct {
    long long inode;
    char name[MAX_NAME_LEN + 1];
} dirent_t;

int f_open(const char* pathname, const char* mode);
ssize_t f_read(int fd, void *buf, size_t count);
ssize_t f_write(int fd, const void *buf, size_t count);
int f_seek(int fd, long offset, int whence);
void f_rewind(int fd);
int f_stat(int fd, inode_t* inode);
int f_remove(int fd);
file_t* f_opendir(const char* pathname);
dirent_t f_readdir(int fd);
int f_closedir(int fd);
int f_mkdir(const char* pathname, const char* mode);
int f_rmdir(const char* pathname);
int f_mount(const char* source, const char* target);
int f_umount(const char* target);

#endif
