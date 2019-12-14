#ifndef FS_ERROR_H
#define FS_ERROR_H

#define SUCCESS 0
#define FAILURE -1

typedef enum {
    // general
    INVALID_PATH = 1,
    NOT_DIR,
    INVALID_SOURCE,
    TARGET_EXISTS,
    INVALID_FD,
    DISK_FULL,

    // fopen, fopendir
    FT_EXCEEDED, // max # of opened files reached

    // fread
    F_EOF,

    // fmount
    DISKS_EXCEEDED, // max # of mounted disks reached
} errno;

extern errno error;

#endif
