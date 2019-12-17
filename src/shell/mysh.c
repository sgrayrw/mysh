#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
int fd_in, fd_out, in, out;

int main() {
    jobs = NULL;
    initialize_handlers(); // register for signal handlers
    tcgetattr(STDIN_FILENO, &mysh_tc);

    init();
    login();

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
        if (line[position] == '>' && line[position + 1] == '>') {
            length = 2;
        } else {
            length = 1;
        }
    } else {
        while (line[position] != '\0' && strchr(DELIMITERS, line[position]) == NULL) {
            position++;
            length++;
        }
    }
    return length;
}

bool check_redirection(int start, int end, bool background) {
    argc = 0;
    args = malloc(sizeof(char *));
    args[0] = "";
    fd_in = FAILURE, fd_out = FAILURE;
    for (int i = start; i <= end; i++) {
        if ((tokens[i][0] == '<' || tokens[i][0] == '>') && background) {
            fprintf(stderr, "Redirection for background jobs is not supported\n");
            return false;
        }
        if (tokens[i][0] == '<') {
            i++;
            if (i > end) {
                fprintf(stderr, "Syntax error near unexpected token '<'\n");
                return false;
            } else {
                if (fd_in != FAILURE) {
                    f_close(fd_in);
                }
                fd_in = f_open(tokens[i], "r");
                if (fd_in == FAILURE) {
                    //TODO error handling
                    return false;
                }
            }
        } else if (tokens[i][0] == '>' && tokens[i][1] == 0) {
            i++;
            if (i > end) {
                fprintf(stderr, "Syntax error near unexpected token '>'\n");
                return false;
            } else {
                if (fd_out != FAILURE) {
                    f_close(fd_out);
                }
                fd_out = f_open(tokens[i], "w");
                if (fd_out == FAILURE) {
                    //TODO error handling
                    return false;
                }
            }
        } else if (tokens[i][0] == '>' && tokens[i][1] == '>') {
            i++;
            if (i > end) {
                fprintf(stderr, "Syntax error near unexpected token '>>'\n");
                return false;
            } else {
                if (fd_out != FAILURE) {
                    f_close(fd_out);
                }
                fd_out = f_open(tokens[i], "a");
                if (fd_out == FAILURE) {
                    //TODO error handling
                    return false;
                }
            }
        } else {
            argc++;
            args = realloc(args, sizeof(char *) * (argc + 1));
            args[argc - 1] = tokens[i];
            args[argc] = "";
        }
    }
    return true;
}

void eval() {
    int i, start_pos = 0, end_pos;
    bool background, punctuation;
    if (line == NULL) {
        return;
    }
    bool launch;
    for (i = 0; i < tokens_len; i++) {
        background = tokens[i][0] == '&';
        punctuation = tokens[i][0] == ';' || background;
        if (punctuation || i == tokens_len - 1) {
            if ((i == tokens_len - 1 && !punctuation) || i > start_pos) {
                if (i == tokens_len - 1 && !punctuation){
                    end_pos = i;
                } else {
                    end_pos = i - 1;
                }
                launch = check_redirection(start_pos, end_pos, background);
                if (strcmp(args[0], "jobs") == 0) {
                    print = false;
                }
                if (launch) launch_process(background, fd_in != FAILURE, fd_out != FAILURE);
                free(args);
                args = NULL;
            }
            start_pos = i + 1;
        }
    }
}

void redirection_pre_launch() {
    if (fd_in != FAILURE) {
        in = open("in", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
        char buffer[BUFSIZE];
        size_t n;
        while ((n = f_read(fd_in, buffer, BUFSIZE)) > 0) {
            write(in, buffer, n);
        }
        f_close(fd_in);
        close(in);
        in = open("in", O_RDONLY);
    }
    if (fd_out != FAILURE) {
        out = open("out", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
    }
}

void redirection_post_launch() {
    if (fd_in != FAILURE) {
        close(in);
    }
    if (fd_out != FAILURE) {
        close(out);
        out = open("out", O_RDONLY);
        char buffer[BUFSIZE];
        size_t n;
        while ((n = read(out, buffer, BUFSIZE)) > 0) {
            f_write(fd_out, buffer, n);
        }
        f_close(fd_out);
        close(out);
    }
}

void launch_process(bool background, bool redirect_in, bool redirect_out) {
    int i, status, jid;
    pid_t pid;

    if (strcmp(args[0], "exit") == 0) {
        my_exit();
    }

    redirection_pre_launch();

    pid = fork();

    if (pid == 0) { // child
        for (i = SIG_MIN; i < NSIG; i++) {
            signal(i, SIG_DFL);
        }
        setpgrp();

        if (redirect_in || redirect_out) {
            signal(SIGTSTP, SIG_IGN);
        }
        if (redirect_in) {
            dup2(in, STDIN_FILENO);
            close(in);
        }
        if (redirect_out) {
            dup2(out, STDOUT_FILENO);
            close(out);
        }

        if (builtin(args, argc) == true) {
            free_list();
            free_tokens();
            term();
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
            term();
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

            redirection_post_launch();

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

    user_id = ID_SUPERUSER;
    int fd = f_opendir(USER);
    if (fd == FAILURE) {
        user_id = ID_USER;
        f_mkdir(USER, "rw--", true);
        printf("created user\n");
        //TODO stats, owner
    } else {
        f_closedir(fd);
    }

    user_id = ID_SUPERUSER;
    fd = f_opendir(SUPERUSER);
    if (fd == FAILURE) {
        f_mkdir(SUPERUSER, "rw--", true);
        printf("created superuser\n");
        //TODO stats, owner
    } else {
        f_closedir(fd);
    }

    char *user = NULL, *pwd = NULL;
    size_t a = 0, b = 0;
    printf("Username: ");
    getline(&user, &a, stdin);
    printf("Password: ");
    getline(&pwd, &b, stdin);
    if (strcmp(user, USER"\n") == 0 && strcmp(pwd, USER_PWD"\n") == 0) {
        user_id = ID_USER;
    } else if (strcmp(user, SUPERUSER"\n") == 0 && strcmp(pwd, SUPERUSER_PWD"\n") == 0) {
        user_id = ID_SUPERUSER;
    } else {
        printf("Invalid username or password.\n");
        term();
        exit(EXIT_SUCCESS);
    }
    free(user);
    free(pwd);
    if (user_id == ID_SUPERUSER) {
        set_wd(SUPERUSER);
    } else {
        set_wd(USER);
    }
}
