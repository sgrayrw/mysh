#ifndef FS_H
#define FS_H

#include <sys/types.h>

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
