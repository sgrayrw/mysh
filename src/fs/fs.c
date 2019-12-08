#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "disk.h"

vnode_t* vnodes; // root of vnode tree

int f_open(const char* pathname, const char* mode) {
    char* name;
    char* next;
    name = strtok(pathname, )
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

file_t* f_opendir(const char* pathname) {

}

dirent_t f_readdir(int fd) {

}

int f_closedir(int fd) {

}

int f_mkdir(const char* pathname, const char* mode) {

}

int f_rmdir(const char* pathname) {

}

int f_mount(const char* source, const char* target) {
    // search for disk file `source` OUTSIDE the shell
    FILE* disk = fopen(source, "r");
    if (!disk) {
        printf("mount: disk %s does not exist\n", source);
        return -1;
    }

    char** tokens;
    int len = split_path(target, &tokens);
    vnode_t* cur = vnodes;
    for (int i = 0; i < len; ++i) {
        cur = find_vnode(cur, )
    }


}

int f_umount(const char* target) {

}

int split_path(const char* pathname, char*** result) {
    char* name;
    char* delim = "/";
    int count = 0;
    if ((name = strtok(pathname, delim)) == NULL){
        return count;
    }
    *result = malloc(sizeof(char*));
    **result = malloc(strlen(name));
    strcpy(**result, name);
    count += 1;
    while((name = strtok(NULL, delim)) != NULL){
        count += 1;
        *result = realloc(*result,sizeof(char*)*count);
        *((*result)+count-1) = malloc(strlen(name));
        strcpy(*((*result)+count-1), name);
    }
    return count;
}
