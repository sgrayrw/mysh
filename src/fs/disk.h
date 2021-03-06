#ifndef DISK_H
#define DISK_H

#define MB (1024 * 1024)
#define DFT_DISKSIZE (1 * MB) /* 1 MB */
#define BOOTSIZE 512
#define SUPERSIZE 512
#define BLOCKSIZE 512
#define N_INODES 128 /* max 128 inodes */
#define N_DBLOCKS 10
#define MAX_NAME_LEN (BLOCKSIZE / 2 - sizeof(long long) - sizeof(f_type) - 1)
#define LEN_PERMISSION 4

typedef enum {DIR, F, EMPTY} f_type;

typedef struct sb_t {
    int block_size;	// 512
    long long inode_start; // 1024
    long long data_start;
    long long free_inode;
    long long free_block;
    long long root_inode;
} sb_t;

typedef struct inode_t {
    long long next_inode; /* position of next free inode */
    char permission[LEN_PERMISSION];
    long long size; /* number of bytes in file */
    int dir_size; /* number of entries in directory */
    f_type type;
    int uid; /* owner’s user ID */
    time_t mtime; /* modification time */
    long long dblocks[N_DBLOCKS];
    long long iblock;
    long long i2block;
    long long i3block;
    long long i4block;
} inode_t;

typedef struct dirent_t {
    long long inode;
    char name[MAX_NAME_LEN + 1];
    f_type type;
} dirent_t;

#endif
