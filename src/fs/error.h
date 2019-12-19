#ifndef FS_ERROR_H
#define FS_ERROR_H

#define SUCCESS 0
#define FAILURE -1

typedef enum {
    // general
    INVALID_PATH = 1,
    NOT_DIR,
    NOT_FILE,
    INVALID_SOURCE,
    TARGET_EXISTS,
    INVALID_FD,
    DISK_FULL,
    PERM_DENIED,

    // rmdir, umount
    NOT_MP, // trying to umount non-mountpoint
    RMDIR_MP, // cannot rmdir mountpoint
    RMDIR_ROOT, // cannot rmdir root

    // fread, fwrite
    MODE_ERR,

    // fopen, fopendir
    FT_EXCEEDED, // max # of opened files reached

    // fread
    F_EOF,

    // fmount
    DISKS_EXCEEDED, // max # of mounted disks reached
} ERRNO;

extern ERRNO error;

#endif
