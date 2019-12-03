#ifndef DISK_H
#define DISK_H

/*
 * disk layout
 * ----------------------
 * boot block - 512
 * ----------------------
 * super block - 512
 * ----------------------
 * inode
 * ----------------------
 * data - block size 512
 * ----------------------
 *
 * directory layout
 * block:
 * inode | filename | inode | filename | ...
 */

#define BOOT_SIZE 512
#define SUPER_SIZE 512
#define N_DBLOCKS 10

typedef struct {
	int block_size;	// 512
  	int inode_offset;
  	int data_offset;
  	int free_inode;
  	int free_block;
} Superblock;

typedef struct {
    int next_inode; /* index of next free inode */
    int permission; /* pen field */
    int size; /* numer of bytes in file */
    int uid; /* owner’s user ID */
    int ctime; /* change time */
    int mtime; /* modification time */
    int atime; /* access time */
    int dblocks[N_DBLOCKS]; /* pointers to data blocks */
    int iblock; /* pointer to single indirect block */
    int i2block; /* pointer to double indirect block */
    int i3block; /* pointer to triple indirect block */
  	int i4block; /* pointer to quadruple indirect block */
  	/* allows file size up to 138 GB */
} Inode;

#endif
