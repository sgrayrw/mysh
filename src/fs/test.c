#include <stdio.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

int user_id = ID_SUPERUSER;

int main() {
    init();
    f_mount("DISK", "/");
    int f = f_open("/ray", "w");
    char* str = "hello";
    f_write(f, str, 6);
    f_close(f);

    f = f_open("/ray", "r");
    char str2[6];
    f_read(f, str2, 5);
    str2[5] = '\0';
    printf("%s\n", str2);

    dump();
    term();
}