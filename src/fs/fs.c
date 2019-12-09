#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "disk.h"


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
    char** tokens;
    int length = split_path(target, &tokens);
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; ++i) {
        parentdir = get_vnode(parentdir, tokens[i]);
        if (!parentdir) {
            printf("mount: invalid mount path %s\n", target);
            return FAILURE;
        }
    }
    if (get_vnode(parentdir, tokens[length - 1])) {
        printf("mount: mount point %s already exists\n", tokens[length - 1]);
        return FAILURE;
    }

    // load disk
    if (n_disks == MAX_DISKS) {
        printf("mount: max number of mounted disks (10) reached\n");
        return FAILURE;
    }
    FILE* disk = fopen(source, "r");
    if (!disk) {
        printf("mount: disk %s does not exist\n", source);
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
    strcpy(mountpoint->name, tokens[length-1]);
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
    // TODO: free
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
    // find from existing vnodes
    vnode_t* cur = parentdir->children;
    if (cur) {
        do {
            if (strcmp(cur->name, filename) == 0)
                return cur;
            cur = cur->next;
        } while (cur != parentdir->children);
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
    return FAILURE;
}
