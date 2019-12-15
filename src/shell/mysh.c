#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mysh.h"
#include "job.h"
#include "builtin.h"
#include "sighand.h"
#include "../fs/fs.h"
#include "../fs/error.h"

// global vars
bool print;
char *line; // dynamically allocated in read_line()
char **tokens, **args; // dynamically allocated in parse_line()
int argc, tokens_len;
struct Node* jobs = NULL;
struct Node* logic_jobs = NULL;
int jobcnt = 0;
struct termios mysh_tc;

int user_id = 0;

int main() {

    login();

    jobs = NULL;
    initialize_handlers(); // register for signal handlers
    tcgetattr(STDIN_FILENO, &mysh_tc);

    while (true) {

        process_changed_jobs(print);
        print = true;

        read_line(); // read into line buffer
        parse_line(); // parse arguments
        eval(); // evaluate arguments
        free_tokens();
    }
}

void read_line() {
    size_t n = 0;
    printf("mysh ❯ "); //TODO change prompt
    if (getline(&line, &n, stdin) == -1) {
        if (feof(stdin)) {
            my_exit();
        } else if (errno == EINTR) {
            clearerr(stdin);
            if (line) free(line);
            line = NULL;
        } else {
            fprintf(stderr, "Error getting input. Exiting shell.\n");
            my_exit();
        }
    }
}

void parse_line() {
    int position = 0, length, n = 0, i = 0;
    char *token;
    if (line == NULL) {
        return;
    }
    while ((length = next_token_length(position)) != 0) {
        if (!isspace(line[position])) {
            n++;
        }
        position += length;
    }
    tokens = malloc(sizeof(char*) * (n + 1));
    position = 0;
    while ((length = next_token_length(position)) != 0) {
        if (!isspace(line[position])) {
            token = malloc(sizeof(char) * (length + 1));
            strncpy(token, &line[position], length);
            token[length] = '\0';
            tokens[i++] = token;
        }
        position += length;
    }
    tokens[n] = NULL;
    tokens_len = n;
}

int next_token_length(int position) {
    int length = 0;
    if (line[position] == '\0') {
        length = 0;
    } else if (strchr(DELIMITERS, line[position]) != NULL) {
        length = 1;
    } else {
        while (line[position] != '\0' && strchr(DELIMITERS, line[position]) == NULL) {
            position++;
            length++;
        }
    }
    return length;
}

void eval() {
    int i, start_pos = 0, end_pos;
    bool background, is_semicolon;
    if (line == NULL) {
        return;
    }
    for (i = 0; i < tokens_len; i++) {
        is_semicolon = strcmp(tokens[i], ";") == 0;
        if (is_semicolon || i == tokens_len - 1) {
            if ((i == tokens_len - 1 && !is_semicolon) || i > start_pos) {
                if (i == tokens_len - 1 && !is_semicolon){
                    end_pos = i;
                } else {
                    end_pos = i - 1;
                }
                argc = end_pos - start_pos + 1;
                args = malloc(sizeof(char *) * (argc + 1));
                memcpy(args, &tokens[start_pos], sizeof(char *) * argc);
                args[argc] = NULL;
                if (strcmp(args[argc - 1], "&") == 0) {
                    argc--;
                    args[argc] = NULL;
                    background = true;
                } else {
                    background = false;
                }
                if (strcmp(args[0], "jobs") == 0) {
                    print = false;
                }
                launch_process(background);
                free(args);
                args = NULL;
            }
            start_pos = i + 1;
        }
    }
}

void launch_process(bool background) {
    int i, status, jid;
    pid_t pid;

    pid = fork();

    if (pid == 0) { // child
        for (i = SIG_MIN; i < NSIG; i++) {
            signal(i, SIG_DFL);
        }
        setpgrp();

        if (builtin(args, argc) == true) {
            free_list();
            free_tokens();
            exit(EXIT_SUCCESS);
        }

        if (execvp(args[0], args) == -1) {
            if (errno == ENOENT) {
                fprintf(stderr, "No such file or directory.\n");
            } else {
                fprintf(stderr, "%s: command not found.\n", tokens[0]);
            }
            free_list();
            free_tokens();
            exit(EXIT_FAILURE);
        }

    } else if (pid > 0) { // parent
        setpgid(pid, pid);
        jid = add_job(pid, Running, argc, args, &mysh_tc);
        if (!background) {
            tcsetpgrp(STDIN_FILENO, pid);
            waitpid(pid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            tcsetattr(STDIN_FILENO, TCSADRAIN, &mysh_tc);
        } else {
            printf("[%d] %d\n", jid, pid);
        }

    } else {
        fprintf(stderr, "Error forking a process.\n");
    }
}

void free_tokens() {
    int i;
    if (tokens != NULL){
        for (i = 0; i < tokens_len; i++) {
            if (tokens[i] != NULL) {
                free(tokens[i]);
                tokens[i] = NULL;
            }
        }
        free(tokens);
        tokens = NULL;
    }
    if (args != NULL) {
        free(args);
        args = NULL;
    }
    if (line != NULL) {
        free(line);
        line = NULL;
    }
}



/******************

hw7

******************/

void login() {
    if (f_mount("DISK", "/") == FAILURE) {
        printf("Failed to detect DISK. Please run the format program to make one.\n");
        exit(EXIT_SUCCESS);
    }
    if (f_opendir(USER) == FAILURE) {
        f_mkdir(USER, "rw--");
        //TODO stats, owner
    } else {
        f_closedir(USER);
    }
    if (f_opendir(SUPERUSER) == FAILURE) {
        f_mkdir(SUPERUSER, "rw--");
        //TODO stats, owner
    } else {
        f_closedir(SUPERUSER);
    }
    char *user, *pwd;
    size_t a = 0, b = 0;
    while (user_id == 0) {
        printf("Username: ");
        getline(&user, &a, stdin);
        printf("Password: ");
        getline(&pwd, &b, stdin);
        if (strcmp(user, USER"\n") == 0 && strcmp(pwd, USER_PWD"\n") == 0) {
            user_id = ID_USER;
        } else if (strcmp(user, SUPERUSER"\n") == 0 && strcmp(pwd, SUPERUSER_PWD)) {
            user_id = ID_SUPERUSER;
        } else {
            printf("Invalid username or password.\n");
        }
    }
    free(user);
    free(pwd);
    if (user_id == ID_SUPERUSER) {
        set_wd(SUPERUSER);
    } else {
        set_wd(USER);
    }
}
