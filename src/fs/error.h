#ifndef FS_ERROR_H
#define FS_ERROR_H

#define SUCCESS 0
#define FAILURE -1

typedef enum {
    INVALID_PATH = 1,
    INVALID_SOURCE,
    TARGET_EXISTS,
    DISKS_EXCEEED,
} errno;

extern errno error;

#endif
