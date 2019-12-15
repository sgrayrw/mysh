#ifndef MYSH_H
#define MYSH_H

#include <stdbool.h>

#define DELIMITERS ";& \f\n\r\t\v"

// main loop
void read_line(); // read into line buffer
void parse_line(); // parse arguments with delimiters
int next_token_length(int); // helper function for parse_line()
void eval(); // evaluate tokens and call builtin/exec
void launch_process(bool);
void free_tokens();

// hw7 functions
void login();

extern int user_id;

#define USER "John Doe"
#define SUPERUSER "Admin"
#define USER_PWD "000000"
#define SUPERUSER_PWD "666666"
#define ID_SUPERUSER 1
#define ID_USER 2

char *user_table[3] = {"", SUPERUSER, USER};

#endif
