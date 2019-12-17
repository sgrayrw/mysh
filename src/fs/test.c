#include <stdio.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

int user_id = ID_SUPERUSER;

int main() {
    init();
    f_mount("DISK", "/");
    int f = f_open("/file", "w");
    f_close(f);

    f_mkdir("/dir", "rw--");
    f = f_opendir("/dir");
    f_closedir(f);

    dump();
    term();
}