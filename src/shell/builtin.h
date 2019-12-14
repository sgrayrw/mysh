#ifndef BUILTIN_H
#define BUILTIN_H

extern struct termios mysh_tc;

// builtin functions
int builtin(char** neededtokens, int argclength);
void my_exit();
void my_jobs(); // list all background jobs
void my_kill(); // kill a background job
void my_fg(); // put a specific job to the foreground
void my_bg(); // continue a specific job in the background
void my_mount();
void my_ls();
void my_cd();
void my_rm();
void my_cat();
void my_pwd();
void my_more();
void my_mkdir();
void my_chmod();
void my_rmdir();
void my_umount();

#endif
