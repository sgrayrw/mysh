#include <stdio.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

int user_id = ID_SUPERUSER;

int main() {
    init();
    f_mount("DISK", "/");
    f_mkdir("ray", "rw--", false);
    f_mkdir("dxu", "rw--", false);
    f_mkdir("/ray/hw1", "rw--", false);
    f_close(f_open("/ray/hw1/file1.txt", "w"));
    f_close(f_open("/ray/hw1/file2.txt", "w"));

    int dir = f_opendir("/ray/hw1");
    inode_t inode;
    char filename[MAX_NAME_LEN + 1];
    printf("fd %d\n", dir);
    while (f_readdir(dir, filename, &inode) != FAILURE)
        printf("entry %s\n", filename);

    dump();
    term();
}