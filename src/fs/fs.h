#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include "disk.h"

#define SUCCESS 0
#define FAILURE -1
#define MAX_NAME_LEN (BLOCKSIZE / 2 - sizeof(long long) - 1)
#define N_DISKS 16

typedef struct {
    long long inode;
    int disk;
    char name[MAX_NAME_LEN + 1];
    struct vnode_t* parent;
    struct vnode_t* children;
    struct vnode_t* next;
} vnode_t;

typedef struct {
    vnode_t vnode;
    long long position; // file position indicator
} file_t;

typedef struct {
    long long inode;
    char name[MAX_NAME_LEN + 1];
} dirent_t;

vnode_t* vnodes; // root of vnode tree
sb_t superblock[N_DISKS];
FILE* disks[N_DISKS];

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

int split_path(const char*, char***);
vnode_t* get_vnode(vnode_t* parent, char* filename);

#endif
