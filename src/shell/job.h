#ifndef JOB_H
#define JOB_H

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <stdbool.h>

typedef enum {
    Running,
    Suspended,
    Done,
    Terminated
} Status;

struct Job {
    int jid; // job id
    pid_t pid;
    Status status;
    int argc;
    char** args; // args that started the job
    struct termios* tcattr;
    bool status_changed;
    bool exited_in_fg;
};

// job related
struct Node {
    struct Job* job;
    struct Node* next;
    struct Node* prev;
    struct Node* logic_next;
    struct Node* logic_prev;
};

extern struct Node* jobs;
extern struct Node* logic_jobs;
extern int jobcnt;

struct Node* get_node_jid(int jid);
int add_job(pid_t pid, Status status, int argc, char** args, struct termios* tcattr);
void remove_job(struct Node* node);
void change_job_status(pid_t pid, Status status, struct termios* tcattr);
void exited_in_fg(pid_t pid);
void process_changed_jobs(bool print);
void print_job(struct Job* job, bool builtin);
void free_node(struct Node* node);
void free_list();

void logic_update(struct Node* node);

#endif
