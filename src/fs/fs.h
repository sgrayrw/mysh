#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include <stdbool.h>
#include "disk.h"

#define MAX_DISKS 16
#define MAX_OPENFILE 256

typedef struct vnode_t {
    long long inode;
    int disk;
    char name[MAX_NAME_LEN + 1];
    f_type type;
    struct vnode_t* parent;
    struct vnode_t* children;
    struct vnode_t* next;
    struct vnode_t* prev;
} vnode_t;

typedef enum {RDONLY, WRONLY, RDWR} f_mode;

typedef struct file_t {
    vnode_t* vnode;
    long long position; // file position indicator
    f_mode mode;
} file_t;

extern char* wd;

int f_open(const char* pathname, const char* mode);
int f_close(int fd);
ssize_t f_read(int fd, void* buf, size_t count);
ssize_t f_write(int fd, void* buf, size_t count);
int f_seek(int fd, long offset, int whence);
int f_rewind(int fd);
int f_stat(const char* pathname, inode_t* inode);
int f_remove(int fd);
int f_opendir(const char* pathname);
int f_readdir(int fd, char** filename, inode_t* inode);
int f_closedir(int fd);
int f_mkdir(const char* pathname, char* mode);
int f_rmdir(const char* pathname);
int f_mount(const char* source, const char* target);
int f_umount(const char* target);
int f_chmod(const char* pathname, char* mode);

void init();
void term();
void dump(); // dump vnode tree for debugging
int set_wd(const char* pathname);

static void dump_vnode(vnode_t* vnode, int depth);
static int split_path(const char* pathname, char*** tokens);
static void free_path(char** path);
static vnode_t* get_vnode(vnode_t* parentdir, char* filename);

static void fetch_inode(vnode_t* vnode, inode_t* inode);
static void update_inode(vnode_t* vnode, inode_t* inode);

static int readdir(vnode_t* dir, int n, dirent_t* dirent, inode_t* inode); // n: return the nth dirent
static vnode_t* create_file(vnode_t* parent, char* filename, f_type type, char* mode);
static long long get_block(int n_disk);
static long long get_inode(int n_disk);
static void cleandata(vnode_t* vnode);

static void free_block(int n_disk, long long address);
static void free_inode(vnode_t* vnode);
static void free_vnode(vnode_t* vnode);

static long long get_block_address(vnode_t* vnode, long long block_number);
static int get_block_index(long long block_number,int* index);
static bool has_permission(vnode_t* vnode, char mode);

#endif
