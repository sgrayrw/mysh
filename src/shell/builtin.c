#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>

#include "builtin.h"
#include "job.h"
#include "mysh.h"

struct Node* lastnode;
char** currenttokens;
int length;

int getlastnode();
int getlastnode_sus();

int builtin(char** neededtokens, int argclength){
    currenttokens = neededtokens;
    length = argclength;
    if (length > 0 ){
        if (strcmp(currenttokens[0],"jobs")==0){
            my_jobs();
            return true;
        } else if (strcmp(currenttokens[0],"exit")==0) {
            my_exit();
            return true;
        } else if (strcmp(currenttokens[0],"kill")==0) {
            my_kill();
            return true;
        } else if (strcmp(currenttokens[0],"bg")==0) {
            my_bg();
            return true;
        } else if (strcmp(currenttokens[0],"fg")==0) {
            my_fg();
            return true;
        }
    }
    return false;
}

void my_jobs(){
    if(!jobs){
        return;
    }
    struct Node* currentjobs = jobs->prev;
    for(int i  = 0; i<jobcnt;i++){
        print_job(currentjobs->job, false);
        currentjobs = currentjobs->prev;
    }
}

void my_exit(){
    free_list();
    free_tokens();
    printf("Goodbye~\n");
    exit(EXIT_SUCCESS);
}

void my_kill(){
    if (length == 1 || (length == 2 && strcmp(currenttokens[length-1],"-9")==0)){
        printf("not enough arguments\n");
        return;
    }

    int argc_start = 1;
    int lastjob_done = false;
    int CURRENTSIG=SIGTERM;
    struct Job* currentjob;
    int currentpid;
    int jobid;

    if (strcmp(currenttokens[1],"-9")==0){
        CURRENTSIG = SIGKILL;
        argc_start ++;
    }

    for (int i = argc_start; i<length;i++){
        if (currenttokens[i][0] != '%'){
            printf("kill: illegal argument: %s\n",currenttokens[i]);
            return;
        }

        if (strcmp(currenttokens[i],"%")==0){
            if (lastjob_done == true){
                continue;
            }
            lastjob_done = true;
            if(getlastnode()==false){
                return;
            }
            currentpid = lastnode->job->pid;
        }else{
            if ((jobid = atoi(currenttokens[i]+1))==0){
                printf("kill: illegal pid: %s\n", currenttokens[i]);
                return;
            };
            if(get_node_jid(jobid)==NULL){
                printf("kill: no such job: %s\n", currenttokens[i]);
                return;
            }else{
                currentjob = get_node_jid(jobid)->job;
            }
            currentpid = currentjob->pid;
        }
        if(kill(currentpid,CURRENTSIG)!=0){
            fprintf(stderr, "Error sending signal\n");
        }

    }
}

void my_fg(){
    int currentpid;
    int jobid;
    struct Node* currentnode;
    struct Job *currentjob;
    if (length == 1 || strcmp(currenttokens[1],"%")==0){
        if(getlastnode()==false){
            return;
        }
        currentnode = lastnode;
        currentjob = currentnode->job;
    } else if (currenttokens[1][0] != '%'){
        printf("fg: illegal argument: %s\n",currenttokens[1]);
        return;
    } else {
        if ((jobid = atoi(currenttokens[1] + 1))==0){
            printf("fg: illegal pid: %s\n", currenttokens[1]);
            return;
        }
        if (get_node_jid(jobid) == NULL) {
            printf("fg: no such job: %s\n", currenttokens[1]);
            return;
        } else {
            currentnode = get_node_jid(jobid);
            currentjob = currentnode->job;
        }
    }
    logic_update(currentnode);
    currentpid = currentjob->pid;
    struct termios* currenttermios = currentjob->tcattr;
    int statusnum;
    int* status = &statusnum;
    kill(currentpid,SIGCONT);
    waitpid(currentpid,status,WCONTINUED | WNOHANG);
    tcsetpgrp(STDIN_FILENO,currentpid);
    tcsetattr(STDIN_FILENO,TCSADRAIN,currenttermios);
    waitpid(currentpid,status,WUNTRACED);
    tcsetpgrp(STDIN_FILENO, getpid());
    tcsetattr(STDIN_FILENO, TCSADRAIN, &mysh_tc);
}

void my_bg(){
    int lastjob_done = false;
    struct Node* currentnode;
    struct Job* currentjob;
    int jobid;
    int currentpid;

    if (length == 1){
        if(getlastnode_sus()==false){
            return;
        }
        currentjob = lastnode->job;
        currentpid = currentjob->pid;
        print_job(currentjob, true);
        kill(currentpid,SIGCONT);
        logic_update(lastnode);
        return;
    }

    for (int i = 1; i<length;i++){
        if (currenttokens[i][0] != '%') {
            printf("bg: illegal argument: %s\n", currenttokens[i]);
            return;
        }
        if (strcmp(currenttokens[i],"%")==0){
            if (lastjob_done == true){
                continue;
            }
            lastjob_done = true;
            if(getlastnode_sus()==false){
                return;
            }
            currentnode = lastnode;
            currentjob = currentnode->job;
            currentpid = lastnode->job->pid;
        }else {
            if((jobid = atoi(currenttokens[i] + 1))==0){
                printf("bg: illegal pid: %s\n", currenttokens[i]);
                return;
            }
            if (get_node_jid(jobid) == NULL) {
                printf("bg: no such job: %s\n", currenttokens[i]);
                return;
            } else {
                currentnode = get_node_jid(jobid);
                currentjob = currentnode->job;
            }
            currentpid = currentjob->pid;
        }
        //printf("jobid:%d\n",currentjob->job->jid);
        logic_update(currentnode);
        print_job(currentjob,true);
        kill(currentpid,SIGCONT);
    }
}

int getlastnode(){
    if (jobs == NULL){
        printf("No current job\n");
        return false;
    }
    struct Node* currentjobs = logic_jobs;
    lastnode = currentjobs;
    return true;
}

int getlastnode_sus(){
    if (jobs == NULL){
        printf("No current job\n");
        return false;
    }
    struct Node* currentjobs = logic_jobs;
    lastnode = currentjobs;
    while(lastnode->job->status != Suspended){
        lastnode = lastnode->next;
        if (lastnode == currentjobs){
            printf("job %d already in background\n", lastnode->prev->job->jid);
            return false;
        }
    }
    return true;
}
