#include <stdio.h>
#include "fs.h"

int main() {
    char* path = "ll/usr";
    char** tokens;
    int len = split_path(path, &tokens);

    printf("len %d\n", len);
    for (int i = 0; i < len; ++i) {
        printf("%s ", tokens[i]);
    }
    printf("\n");
}