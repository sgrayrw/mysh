#ifndef MYSH_H
#define MYSH_H

#define DELIMITERS ";& \f\n\r\t\v"

// main loop
void read_line(); // read into line buffer
void parse_line(); // parse arguments with delimiters
int next_token_length(int); // helper function for parse_line()
void eval(); // evaluate tokens and call builtin/exec
void launch_process(bool);
void free_tokens();

#endif
