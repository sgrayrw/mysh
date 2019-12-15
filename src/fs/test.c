#include <stdio.h>
#include "error.h"
#include "fs.h"

int main() {
    f_mount("DISK", "/");
    f_mkdir("/ray", "rw--");
}