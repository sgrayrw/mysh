#ifndef DISK_H
#define DISK_H

#define MB (1024 * 1024)
#define DFT_DISKSIZE (1 * MB) /* 1 MB */
#define BOOTSIZE 512
#define SUPERSIZE 512
#define BLOCKSIZE 512
#define N_INODES 128 /* max 128 inodes */
#define N_DBLOCKS 10

typedef struct {
    int block_size;	// 512
    long long inode_start; // 1024
    long long data_start;
    long long free_inode;
    long long free_block;
} sb_t;

typedef struct {
    long long next_inode; /* index of next free inode */
    int permission; /* permission field */
    int size; /* numer of bytes in file */
    int uid; /* owner’s user ID */
    int ctime; /* change time */
    int mtime; /* modification time */
    int atime; /* access time */
    long long dblocks[N_DBLOCKS]; /* pointers to data blocks */
    long long iblock; /* pointer to single indirect block */
    long long i2block; /* pointer to double indirect block */
    long long i3block; /* pointer to triple indirect block */
    long long i4block; /* pointer to quadruple indirect block */
} inode_t;

#endif
