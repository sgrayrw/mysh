#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <stdbool.h>

#include "job.h"

struct Node* get_node_jid(int jid) {
    if (jobs == NULL) {
        return NULL;
    }
    struct Node* node = jobs;
    do {
        if (node->job->jid == jid) {
            return node;
        }
        node = node->next;
    } while (node->job != jobs->job);
    return NULL;
}

int add_job(pid_t pid, Status status, int _argc, char** _args, struct termios* tcattr) {
    struct Job* job = malloc(sizeof(struct Job));
    if (jobs == NULL) {
        job->jid = 1;
    } else {
        job->jid = jobs->job->jid + 1;
    }
    job->pid = pid;
    job->status = status;
    job->argc = _argc;
    job->args = malloc(sizeof(char*) * _argc);
    for (int i = 0; i < _argc; ++i) {
        job->args[i] = malloc(strlen(_args[i]) + 1);
        strcpy(job->args[i], _args[i]);
    }
    job->tcattr = malloc(sizeof(struct termios));
    memcpy(job->tcattr, tcattr, sizeof(struct termios));
    job->status_changed = false;
    job->exited_in_fg = false;

    struct Node* node = malloc(sizeof(struct Node));
    node->job = job;

    // add to head (CS, masking SIGCHLD)
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    if (jobs == NULL) {
        jobs = node;
        jobs->next = jobs;
        jobs->prev = jobs;

        logic_jobs = node;
        logic_jobs->logic_next = logic_jobs;
        logic_jobs->logic_prev = logic_jobs;
    } else {
        node->next = jobs;
        node->prev = jobs->prev;
        jobs->prev->next = node;
        jobs->prev = node;
        jobs = node;

        node->logic_next = logic_jobs;
        node->logic_prev = logic_jobs->logic_prev;
        logic_jobs->logic_prev->logic_next = node;
        logic_jobs->logic_prev = node;
        logic_jobs = node;
    }
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    jobcnt++;
    return job->jid;
}

void remove_job(struct Node* node) {
    if (node == NULL) {
        return;
    }

    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->logic_next->logic_prev = node->logic_prev;
    node->logic_prev->logic_next = node->logic_next;

    struct Node* tmp = node;
    if (logic_jobs->job == node->job) {
        logic_jobs = logic_jobs->logic_next;
        if (logic_jobs->job == node->job) {
            logic_jobs = NULL;
        }
    }
    if (jobs->job == node->job) {
        jobs = jobs->next;
        if (jobs->job == node->job) {
            jobs = NULL;
        }
    }
    free_node(tmp);
    jobcnt--;
}

void change_job_status(pid_t pid, Status status, struct termios* tcattr) {
    if (jobs == NULL) {
        return;
    }
    struct Node* node = jobs;
    do {
        if (node->job->pid == pid) {
            node->job->status = status;
            if (tcattr) {
                free(node->job->tcattr);
                node->job->tcattr = malloc(sizeof(struct termios));
                memcpy(node->job->tcattr, tcattr, sizeof(struct termios));
            }
            node->job->status_changed = true;
            return;
        }
        node = node->next;
    } while (node->job != jobs->job);
}

void exited_in_fg(pid_t pid) {
    if (jobs == NULL) {
        return;
    }
    struct Node* node = jobs;
    do {
        if (node->job->pid == pid) {
            node->job->exited_in_fg = true;
        }
        node = node->next;
    } while (node->job != jobs->job);
}

void unchange_status(pid_t pid) {
    if (jobs == NULL) {
        return;
    }
    struct Node* node = jobs;
    do {
        if (node->job->pid == pid) {
            node->job->status_changed = false;
            return;
        }
        node = node->next;
    } while (node->job != jobs->job);
}

void process_changed_jobs(bool _print) {
    if (jobs == NULL) {
        return;
    }
    struct Node* node = jobs->prev;
    int i = 0;
    int curjobcnt = jobcnt;
    do {
        struct Node* tmp = NULL;
        if (node->job->exited_in_fg) {
            tmp = node;
        } else {
            if (_print && node->job->status_changed) {
                print_job(node->job, false);
            }
            if (node->job->status == Done || node->job->status == Terminated) {
                tmp = node;
            } else {
                unchange_status(node->job->pid);
            }
        }
        node = node->prev;
        remove_job(tmp);
        i++;
    } while (i < curjobcnt);
}

void print_job(struct Job* job, bool builtin) {
    char* statusstr;
    switch (job->status) {
        case Running:
            statusstr = "Running   ";
            break;
        case Suspended:
            statusstr = "Suspended ";
            break;
        case Done:
            statusstr = "Done      ";
            break;
        case Terminated:
            statusstr = "Terminated";
            break;
    }
    if (builtin) {
        printf("[%d]\t\t", job->jid);
    }
    else {
        printf("[%d]\t%s\t", job->jid, statusstr);
    }
    for (int i = 0; i < job->argc; ++i){
        printf("%s ", job->args[i]);
    }
    printf("\n");
}

void free_node(struct Node* node) {
    free(node->job->tcattr);
    node->job->tcattr = NULL;
    for (int i = 0; i < node->job->argc; ++i) {
        free(node->job->args[i]);
        node->job->args[i] = NULL;
    }
    free(node->job->args);
    node->job->args = NULL;
    free(node->job);
    node->job = NULL;
    free(node);
}

void free_list() {
    int curjobcnt = jobcnt;
    for (int i = 0; i < curjobcnt; ++i) {
        remove_job(jobs);
    }
}

void logic_update(struct Node* node) {
    if (node == logic_jobs) {
        return;
    } else if (node->logic_next==node->logic_prev) {
        logic_jobs = node;
    } else {
        node->logic_prev->logic_next = node->logic_next;
        node->logic_next->logic_prev = node->logic_prev;
        node->logic_next = logic_jobs;
        node->logic_prev = logic_jobs->logic_prev;
        logic_jobs->logic_prev->logic_next = node;
        logic_jobs->logic_prev = node;
        logic_jobs = node;
    }
}
