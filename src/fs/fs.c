#include "fs.h"

int f_open(const char* pathname, const char* mode)  {

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


}

int f_umount(const char* target) {

}
