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

    f_mkdir("/dir", "rw--", false);
    f = f_opendir("/dir");
    f_closedir(f);
    f = f_open("/dir/tests", "w");
    char* buffer = "abcdefg";
    int fee = f_write(f,(void*)buffer,5);
    printf("%d\n",fee);
    f_close(f);
    f = f_open("/dir/tests", "r");
    fee = f_seek(f,2,SEEK_SET);
    char* buffer2[10];
    printf("%d,error:%d\n",fee,error);
    fee = f_read(f,(void*)buffer2,4);
    buffer2[7] = "\0";
    printf("%d,error:%d\n",fee,error);
    printf("%s\n",buffer2);
    f_close(f);

    dump();
    term();
}