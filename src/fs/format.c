#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "disk.h"

void usage();
void format();

long long disksize = DFT_DISKSIZE;
FILE* out;

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        usage();
    }

    // `-s` custom disk size
    if (argc == 4) {
        if (strcmp(argv[2], "-s") != 0)
            usage();

        char* endptr;
        disksize = strtoll(argv[3], &endptr, 10) * MB;
        if (disksize <= 0 || disksize == LLONG_MIN || disksize == LLONG_MAX ||
            *endptr != '\0') {
            printf("Invalid size: %s\n\n", argv[3]);
            usage();
        }
    }

    // output file
    out = fopen(argv[1], "w");
    if (!out) {
        perror(argv[1]);
        exit(EXIT_FAILURE);
    }

    // format disk
    format();
    fclose(out);
    printf("formatted disk: <DISK> with size %lld MB\n", disksize / MB);
    exit(EXIT_SUCCESS);
}

void usage() {
    printf("Usage:\n"
           "./format <filename> [option]\n"
           "option:\n"
           "-s #\t\t#: disk size (MB)\n");
    exit(EXIT_FAILURE);
}

void format() {
    // bootblock
    while (ftell(out) < BOOTSIZE) {
        char* boot_content = "boot";
        fwrite(boot_content, strlen(boot_content), 1, out);
    }

    // superblock
    sb_t sb;
    sb.block_size = BLOCKSIZE;
    sb.inode_start = sb.free_inode = BOOTSIZE + SUPERSIZE;

    // inodes
    fseek(out, sb.inode_start, SEEK_SET);
    for (int i = 0; i < N_INODES; ++i) {
        inode_t inode;
        inode.next_inode = (i == N_INODES - 1) ? (-1) : (sb.inode_start + (i + 1) * (long long) sizeof(inode_t));
        inode.permission = 0;
        inode.size = 0;
        inode.uid = 0;
        inode.ctime = inode.mtime = inode.atime = 0;
        for (int j = 0; j < N_DBLOCKS; ++j) {
            inode.dblocks[j] = 0;
        }
        inode.iblock = inode.i2block = inode.i3block = inode.i4block = 0;
        fwrite(&inode, sizeof(inode_t), 1, out);
    }

    // then write superblock to disk
    long long data_start = ((ftell(out) - 1) / BLOCKSIZE + 1) * BLOCKSIZE; // round up to multiples of BLOCKSIZE
    sb.data_start = sb.free_block = data_start;
    fseek(out, BOOTSIZE, SEEK_SET);
    fwrite(&sb, sizeof(sb_t), 1, out);

    // blocks
    void* buf = calloc(1, BLOCKSIZE);
    fseek(out, sb.data_start, SEEK_SET);
    while (ftell(out) < disksize) {
        long long next_block = (ftell(out) + BLOCKSIZE == disksize) ? (-1) : (ftell(out) + BLOCKSIZE);
        *((long long*) buf) = next_block;
        fwrite(buf, 1, BLOCKSIZE, out);
    }
    free(buf);
}
