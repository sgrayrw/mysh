#include <stdio.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

int user_id = ID_SUPERUSER;

int main() {
    init();
    f_mount("DISK", "/");
    f_open("/ray", "w");
    f_open("/ruikang", "w");
    f_open("/jiyu", "w");
    f_open("/dxu", "w");
    dump();
    term();
}