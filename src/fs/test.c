#include <stdio.h>
#include "fs.h"

int main() {
    f_mount("DISK", "/");
    f_mount("DISK", "/usr");
    f_mount("DISK", "/usr/su");
    f_mount("DISK", "/etc");
    f_mount("DISK", "/usr/rayrw");
    dump();
}