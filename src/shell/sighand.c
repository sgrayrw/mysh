#include <signal.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>

#include "sighand.h"
#include "job.h"

void initialize_handlers() {
    struct sigaction sigint_action = {
            .sa_handler = &sigint_handler,
            .sa_flags = 0
    };
    struct sigaction sigchld_action = {
            .sa_sigaction = &sigchld_handler,
            .sa_flags = SA_RESTART | SA_SIGINFO
    };

    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    sigemptyset(&sigchld_action.sa_mask);
    sigaction(SIGCHLD, &sigchld_action, NULL);

    signal(SIGTERM, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

void sigint_handler(int sig) {
    write(STDOUT_FILENO, "\n", sizeof(char));
}

void sigchld_handler(int sig, siginfo_t *info, void *ucontext) {
    struct termios child_tc;
    pid_t pid;
    pid_t child = info->si_pid;
    int status;
    bool foreground = tcgetpgrp(STDIN_FILENO) == child;
    switch (info->si_code) {
        case CLD_EXITED: case CLD_KILLED: case CLD_DUMPED:
            if (foreground) {
                exited_in_fg(child);
            }
            if (info->si_code == CLD_EXITED) {
                change_job_status(child, Done, NULL);
            } else {
                change_job_status(child, Terminated, NULL);
            }
            break;
        case CLD_STOPPED:
            if (foreground) {
                tcgetattr(STDIN_FILENO, &child_tc);
                change_job_status(child, Suspended, &child_tc);
            } else {
                change_job_status(child, Suspended, NULL);
            }
            break;
        case CLD_CONTINUED:
            change_job_status(child, Running, NULL);
            break;
        default:
            break; //nothing
    }
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFEXITED(status)) {
            change_job_status(pid, Done, NULL);
        } else if (WIFSTOPPED(status)) {
            change_job_status(pid, Suspended, NULL);
        } else {
            change_job_status(pid, Terminated, NULL);
        }
    }
}
