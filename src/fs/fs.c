#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "fs.h"

errno error;
static vnode_t* vnodes; // root of vnode tree
static int n_disks;
static FILE* disks[MAX_DISKS]; // disks mounted
static sb_t* superblocks[MAX_DISKS]; // superblock for each disk

int f_open(const char* pathname, const char* mode) {
    char** path;
    int length = split_path(pathname, &path);
    vnode_t* current = vnodes;
    for (int i = 0; i < length-1; i++){
        char* name = *(path+i);
        if ((current = get_vnode(current, name)) == NULL){
            return FAILURE;
        }
    }
    char* name = *(path+length-1);
    if ((current = get_vnode(current, name)) == NULL){

        return FAILURE;
    }
    return SUCCESS;
}

ssize_t f_read(int fd, void *buf, size_t count) {

}

ssize_t f_write(int fd, const void *buf, size_t count) {

}

int f_seek(int fd, long offset, int whence) {

}

void f_rewind(int fd) {

}

int f_stat(int fd, inode_t* inode) {

}

int f_remove(int fd) {

}

int f_opendir(const char* pathname) {

}

int f_readdir(int fd, dirent_t* dirent) {

}

int f_closedir(int fd) {

}

int f_mkdir(const char* pathname, const char* mode) {

}

int f_rmdir(const char* pathname) {

}

int f_mount(const char* source, const char* target) {
    // check target mount point
    char** path;
    int length = split_path(target, &path);
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; ++i) {
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            return FAILURE;
        }
    }
    if (length > 0 && get_vnode(parentdir, path[length - 1])) {
        error = TARGET_EXISTS;
        return FAILURE;
    }

    // load disk
    if (n_disks == MAX_DISKS) {
        error = DISKS_EXCEEDED;
        return FAILURE;
    }
    FILE* disk = fopen(source, "r");
    if (!disk) {
        error = INVALID_SOURCE;
        return FAILURE;
    }
    disks[n_disks] = disk;
    sb_t* sb = malloc(sizeof(sb_t));
    fread(sb, sizeof(sb_t), 1, disk);
    superblocks[n_disks] = sb;

    // add vnode corresponding to the mount point
    vnode_t* mountpoint = malloc(sizeof(vnode_t));
    mountpoint->inode = sb->root_inode;
    mountpoint->disk = n_disks;
    if (length == 0) {
        // mount at `/` (for the first disk)
        strcpy(mountpoint->name, "/");
        mountpoint->parent = NULL;
        mountpoint->children = NULL;
        mountpoint->next = mountpoint;
        mountpoint->prev = mountpoint;
        vnodes = mountpoint;
    } else {
        strcpy(mountpoint->name, path[length - 1]);
        mountpoint->parent = parentdir;
        mountpoint->children = NULL;
        if (!parentdir->children) {
            parentdir->children = mountpoint;
            mountpoint->next = mountpoint;
            mountpoint->prev = mountpoint;
        } else {
            mountpoint->next = parentdir->children->next;
            mountpoint->next->prev = mountpoint;
            mountpoint->prev = parentdir->children;
            mountpoint->prev->next = mountpoint;
        }
    }
    n_disks++;

    free(path);
    return SUCCESS;
}

int f_umount(const char* target) {

}

void init() {
    vnodes = NULL;
    n_disks = 0;
    for (int i = 0; i < MAX_DISKS; ++i) {
        disks[i] = NULL;
        superblocks[i] = NULL;
    }
}

void term() {
    rm_vnode(vnodes);
    for (int i = 0; i < n_disks; ++i) {
        fclose(disks[i]);
        free(superblocks[i]);
    }
}

void rm_vnode(vnode_t* vnode) {
    // TODO: cleanup tree
}

void dump() {
    if (vnodes)
        dump_vnode(vnodes, 1);
}

void dump_vnode(vnode_t* vnode, int depth) {
    printf("%s\n", vnode->name);
    vnode_t* child = vnode->children;
    if (child) {
        do {
            for (int i = 0; i < depth; ++i)
                printf("|-- ");
            dump_vnode(child, depth + 1);
            child = child->next;
        } while (strcmp(child->name, vnode->children->name) != 0);
    }
}

int split_path(const char* pathname, char*** tokens) {
    char* name;
    char* delim = "/";
    int count = 0;
    char buf[strlen(pathname + 1)];
    strcpy(buf, pathname);
    if ((name = strtok(buf, delim)) == NULL){
        return count;
    }
    *tokens = malloc(sizeof(char*));
    **tokens = malloc(strlen(name) + 1);
    strcpy(**tokens, name);
    count += 1;
    while((name = strtok(NULL, delim)) != NULL){
        count += 1;
        *tokens = realloc(*tokens,sizeof(char*)*count);
        *((*tokens) + count - 1) = malloc(strlen(name) + 1);
        strcpy(*((*tokens) + count - 1), name);
    }
    return count;
}

vnode_t* get_vnode(vnode_t* parentdir, char* filename) {
    // `.` and `..`
    if (strcmp(filename, ".") == 0)
        return parentdir;
    else if (strcmp(filename, "..") == 0)
        return parentdir->parent;

    // find from existing vnodes
    vnode_t* child = parentdir->children;
    if (child) {
        do {
            if (strcmp(child->name, filename) == 0)
                return child;
            child = child->next;
        } while (strcmp(child->name, parentdir->children->name) != 0);
    }

    // load from disk
    dirent_t dirent;
    int n = 0;
    while (readdir(parentdir, n, &dirent) != FAILURE) {
        if (strcmp(dirent.name, filename) != 0) {
            n++;
            continue;
        }

        // found match in disk, create new vnode
        vnode_t* newchild = malloc(sizeof(vnode_t));
        newchild->inode = dirent.inode;
        newchild->disk = parentdir->disk;
        strcpy(newchild->name, dirent.name);
        newchild->parent = parentdir;
        newchild->children = NULL;
        newchild->cur_entry = 0;

        // add to children list of parentdir
        if (!parentdir->children) {
            parentdir->children = newchild;
            newchild->next = newchild;
            newchild->prev = newchild;
        } else {
            newchild->next = parentdir->children->next;
            newchild->next->prev = newchild;
            newchild->prev = parentdir->children;
            newchild->prev->next = newchild;
        }

        return newchild;
    }

    return NULL;
}

int readdir(vnode_t* dir, int n, dirent_t* dirent) {
    inode_t inode;
    FILE* disk = disks[dir->disk];
    fseek(disk, dir->inode, SEEK_SET);
    fread(&inode, sizeof(inode_t), 1, disk);

    if (n > inode.size)
        return FAILURE;

    long long block = 0;
    // dblocks
    for (int i = 0; i < N_DBLOCKS && n >= 0; ++i) {
        block = inode.dblocks[i];
        n -= 2; // two dirents in a block
    }
    // iblock
    fseek(disk, inode.iblock, SEEK_SET);
    while (ftell(disk) - inode.iblock < BLOCKSIZE && n >= 0) {
        fread(&block, sizeof(long long), 1, disk);
        n -= 2;
    }
    // i2block
    fseek(disk, inode.i2block, SEEK_SET);
    while (ftell(disk) - inode.i2block < BLOCKSIZE && n >= 0) {
        long long iblock;
        fread(&iblock, sizeof(long long), 1, disk);
        long loop1 = ftell(disk);

        fseek(disk, iblock, SEEK_SET);
        while (ftell(disk) - iblock < BLOCKSIZE && n >= 0) {
            fread(&block, sizeof(long long), 1, disk);
            n -= 2;
        }
        fseek(disk, loop1, SEEK_SET);
    }
    // i3block
    fseek(disk, inode.i3block, SEEK_SET);
    while (ftell(disk) - inode.i3block < BLOCKSIZE && n >= 0) {
        long long i2block;
        fread(&i2block, sizeof(long long), 1, disk);
        long loop2 = ftell(disk);

        fseek(disk, i2block, SEEK_SET);
        while (ftell(disk) - i2block < BLOCKSIZE && n >= 0) {
            long long iblock;
            fread(&iblock, sizeof(long long), 1, disk);
            long loop1 = ftell(disk);

            fseek(disk, iblock, SEEK_SET);
            while (ftell(disk) - iblock < BLOCKSIZE && n >= 0) {
                fread(&block, sizeof(long long), 1, disk);
                n -= 2;
            }
            fseek(disk, loop1, SEEK_SET);
        }
        fseek(disk, loop2, SEEK_SET);
    }
    // i4block
    fseek(disk, inode.i4block, SEEK_SET);
    while (ftell(disk) - inode.i4block < BLOCKSIZE && n >= 0) {
        long long i3block;
        fread(&i3block, sizeof(long long), 1, disk);
        long loop3 = ftell(disk);

        fseek(disk, i3block, SEEK_SET);
        while (ftell(disk) - i3block < BLOCKSIZE && n >= 0) {
            long long i2block;
            fread(&i2block, sizeof(long long), 1, disk);
            long loop2 = ftell(disk);

            fseek(disk, i2block, SEEK_SET);
            while (ftell(disk) - i2block < BLOCKSIZE && n >= 0) {
                long long iblock;
                fread(&iblock, sizeof(long long), 1, disk);
                long loop1 = ftell(disk);

                fseek(disk, iblock, SEEK_SET);
                while (ftell(disk) - iblock < BLOCKSIZE && n >= 0) {
                    fread(&block, sizeof(long long), 1, disk);
                    n -= 2;
                }
                fseek(disk, loop1, SEEK_SET);
            }
            fseek(disk, loop2, SEEK_SET);
        }
        fseek(disk, loop3, SEEK_SET);
    }

    if (block == 0 || n >= 0)
        return FAILURE;
    else if (n == -2)
        fseek(disk, block, SEEK_SET);
    else
        fseek(disk, block + (long long) sizeof(dirent_t), SEEK_SET);

    fread(dirent, sizeof(dirent_t), 1, disk);
    return SUCCESS;
}

long long get_block(int n_disk) {
    FILE* disk = disks[n_disk];
    sb_t* sb = superblocks[n_disk];

    if (sb->free_block == -1)
        return FAILURE;

    // get next free block
    long long free_block = sb->free_block;

    // update free block list in superblock
    fseek(disk, free_block, SEEK_SET);
    fread(&(sb->free_block), sizeof(long long), 1, disk);

    // update superblock in disk
    fseek(disk, BOOTSIZE, SEEK_SET);
    fwrite(sb, sizeof(sb_t), 1, disk);

    return free_block;
}
