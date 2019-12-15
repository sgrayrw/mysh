#include <stdio.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

int user_id = ID_SUPERUSER;

int main() {
    init();
    f_mount("DISK", "/");
    if (!f_open("/ray", "w"))
        printf("error %d\n", error);
    dump();
    term();
}