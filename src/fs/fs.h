#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include "disk.h"

#define SUCCESS 0
#define FAILURE -1
#define N_DISKS 16

typedef struct vnode_t {
    long long inode;
    int disk;
    char name[MAX_NAME_LEN + 1];
    struct vnode_t* parent;
    struct vnode_t* children;
    struct vnode_t* next;
    struct vnode_t* prev;
    int cur_entry; /* current entry for f_readdir */
} vnode_t;

typedef struct file_t {
    vnode_t vnode;
    long long position; // file position indicator
} file_t;

vnode_t* vnodes; // root of vnode tree
sb_t superblock[N_DISKS];
FILE* disks[N_DISKS];

int f_open(const char* pathname, const char* mode);
ssize_t f_read(int fd, void* buf, size_t count);
ssize_t f_write(int fd, const void *buf, size_t count);
int f_seek(int fd, long offset, int whence);
void f_rewind(int fd);
int f_stat(int fd, inode_t* inode);
int f_remove(int fd);
int f_opendir(const char* pathname);
int f_readdir(int fd, dirent_t* dirent);
int f_closedir(int fd);
int f_mkdir(const char* pathname, const char* mode);
int f_rmdir(const char* pathname);
int f_mount(const char* source, const char* target);
int f_umount(const char* target);

int split_path(const char* pathname, char*** tokens);
vnode_t* get_vnode(vnode_t* parentdir, char* filename);
int readdir(vnode_t* dir, int n, dirent_t* dirent); // n: return the nth dirent

#endif
