#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include "disk.h"

#define MAX_DISKS 16

typedef struct vnode_t {
    long long inode;
    int disk;
    char name[MAX_NAME_LEN + 1];
    f_type type;
    struct vnode_t* parent;
    struct vnode_t* children;
    struct vnode_t* next;
    struct vnode_t* prev;
    int cur_entry; // next entry to return for f_readdir
} vnode_t;

typedef struct file_t {
    vnode_t vnode;
    long long position; // file position indicator
} file_t;

/*
 * IMPORTANT:
 * all `pathname` should be absolute paths starting with `/`
 * e.g. `/usr/su/`
 */
int f_open(const char* pathname, const char* mode);
ssize_t f_read(int fd, void* buf, size_t count);
ssize_t f_write(int fd, const void *buf, size_t count);
int f_seek(int fd, long offset, int whence);
void f_rewind(int fd);
int f_stat(int fd, inode_t* inode);
int f_remove(int fd);
int f_opendir(const char* pathname);
int f_readdir(int fd, char** filename, inode_t* inode);
int f_closedir(int fd);
int f_mkdir(const char* pathname, const char* mode);
int f_rmdir(const char* pathname);
int f_mount(const char* source, const char* target);
int f_umount(const char* target);

void init();
void term();
void dump(); // dump vnode tree for debugging

static void rm_vnode(vnode_t* vnode);
static void dump_vnode(vnode_t* vnode, int depth);
static int split_path(const char* pathname, char*** tokens);
static vnode_t* get_vnode(vnode_t* parentdir, char* filename);
static vnode_t* traverse_path(char** path, int length);
static int readdir(vnode_t* dir, int n, dirent_t* dirent, inode_t* inode); // n: return the nth dirent
static long long get_block(int n_disk);

char *getwd();
int setwd(char *pathname);

#endif
