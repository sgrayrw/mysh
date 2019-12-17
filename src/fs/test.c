#include <stdio.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

int user_id = ID_SUPERUSER;

int main() {
    init();
    /*
    if (f_mount("DISK", "/") == FAILURE) {
        fprintf(stderr, "f_mount(\"DISK\", \"/\") == FAILURE\n");
        return 0;
    }
    if (f_mkdir("ray", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"ray\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    if (f_mkdir("dxu", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"dxu\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    if (f_mkdir("/ray/hw1", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"/ray/hw1\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    int fd1 = f_open("/ray/hw1/file1.txt", "w");
    if (fd1 == FAILURE) {
        fprintf(stderr, "fd1 == FAILURE\n");
        return 0;
    }
    if (f_close(fd1) == FAILURE) {
        fprintf(stderr, "f_close(fd1) == FAILURE\n");
        return 0;
    }
    int fd2 = f_open("/ray/hw1/file2.txt", "w");
    if (fd2 == FAILURE) {
        fprintf(stderr, "fd2 == FAILURE\n");
        return 0;
    }
    if (f_close(fd2) == FAILURE) {
        fprintf(stderr, "f_close(fd2) == FAILURE\n");
        return 0;
    }
    if (f_mount("DISK2", "/mount")== FAILURE) {
        fprintf(stderr, "f_mount(\"DISK2\", \"/mount\")== FAILURE\n");
        return 0;
    }
    if (f_mkdir("/mount/ruikang", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"/mount/ruikang\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    if (f_mkdir("/mount/jiyu", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"/mount/jiyu\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    int fd3 = f_open("/mount/jiyu/final.txt", "w");
    if (fd3 == FAILURE) {
        fprintf(stderr, "fd3 == FAILURE\n");
        return 0;
    }
    if (f_close(fd3) == FAILURE) {
        fprintf(stderr, "f_close(fd3) == FAILURE\n");
        return 0;
    }
    */

    if (f_mount("DISK", "/") == FAILURE) {
        fprintf(stderr, "f_mount(\"DISK\", \"/\") == FAILURE\n");
        return 0;
    }
    if (f_mkdir("ray", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"ray\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    if (f_mkdir("dxu", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"dxu\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    if (set_wd("ray/../ray/./") == FAILURE) {
        fprintf(stderr, "set_wd(\"ray\") == FAILURE\n");
        return 0;
    }
    if (f_mkdir("hw1", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"hw1\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    if (set_wd("hw1") == FAILURE) {
        fprintf(stderr, "set_wd(\"hw1\") == FAILURE");
        return 0;
    }
    int fd1 = f_open("file1.txt", "w");
    if (fd1 == FAILURE) {
        fprintf(stderr, "fd1 == FAILURE\n");
        return 0;
    }
    if (f_close(fd1) == FAILURE) {
        fprintf(stderr, "f_close(fd1) == FAILURE\n");
        return 0;
    }
    int fd2 = f_open("/ray/hw1/file2.txt", "w");
    if (fd2 == FAILURE) {
        fprintf(stderr, "fd2 == FAILURE\n");
        return 0;
    }
    if (f_close(fd2) == FAILURE) {
        fprintf(stderr, "f_close(fd2) == FAILURE\n");
        return 0;
    }
    if (set_wd("/") == FAILURE) {
        fprintf(stderr, "set_wd(\"/\") == FAILURE\n");
        return 0;
    }
    if (f_mount("DISK2", "/mount")== FAILURE) {
        fprintf(stderr, "f_mount(\"DISK2\", \"/mount\")== FAILURE\n");
        return 0;
    }
    if (f_mkdir("/mount/ruikang", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"/mount/ruikang\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    if (f_mkdir("/mount/jiyu", "rw--", false) == FAILURE) {
        fprintf(stderr, "f_mkdir(\"/mount/jiyu\", \"rw--\", false) == FAILURE\n");
        return 0;
    }
    int fd3 = f_open("/mount/jiyu/final.txt", "w");
    if (fd3 == FAILURE) {
        fprintf(stderr, "fd3 == FAILURE\n");
        return 0;
    }
    if (f_close(fd3) == FAILURE) {
        fprintf(stderr, "f_close(fd3) == FAILURE\n");
        return 0;
    }

//    int dir = f_opendir("/ray/hw1");
//    inode_t inode;
//    char filename[MAX_NAME_LEN + 1];
//    printf("fd %d\n", dir);
//    while (f_readdir(dir, filename, &inode) != FAILURE)
//        printf("entry %s\n", filename);

    dump();
    term();
}