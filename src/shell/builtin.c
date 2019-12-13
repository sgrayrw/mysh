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
#include "../fs/fs.h"
#include "../fs/error.h"

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
        if(kill(-currentpid,CURRENTSIG)!=0){
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
    kill(-currentpid,SIGCONT);
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
        kill(-currentpid,SIGCONT);
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
        kill(-currentpid,SIGCONT);
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



/****************************

hw7

issues to think about:
    1. permission
    2. redirection

****************************/



void ls_directory(int fd, mode_F, mode_l) {
    char indicator, **filename;
    inode_t *inode;
    while (f_readdir(fd, filename, inode) == SUCCESS) {  //TODO
        indicator = 0;
        if (mode_F && inode->type == DIR) {
            indicator = '/';
        }
        if (mode_l) {
            //TODO
            printf("%s%c\n", *filename, indicator);
        } else {
            printf("%s%c\t", *filename, indicator);
            printf("\n");
        }
    }
}

void my_ls() {
    bool mode_F = false, mode_l = false, no_arguments = true;
    int index;
    for (index = 1; index < length; index++) {
        if (currenttokens[index][0] == '_' && strlen(currenttokens[index] > 1)) {
            for (int i = 1; i < strlen(currenttokens[index]); i++) {
                if (currenttokens[index][i] == 'F') {
                    mode_F = true;
                } else if (currenttokens[index][i] == 'l') {
                    mode_l = true;
                } else {
                    fprintf(stderr, "ls: invalid option -- '%c'\n");
                    return;
                }
            }
        } else {
            no_arguments = false;
        }
    }

    int fd;

    if (no_arguments) {
        fd = f_opendir(wd);
        if (fd == FAILURE) {
            fprintf(stderr, "Exception occurred when trying to open the current directory\n");
        }
        ls_directory(fd, mode_F, mode_l);
        f_closedir(fd);

    } else {
        bool first_argument = true;
        char *path;
        int path_length;
        inode_t stats;
        for (index = 1; index < length; index++) {
            path = currenttokens[index];
            path_length = strlen(path);
            if (path_length == 1 || path[0] != '_') {

                if (!first_argument) {
                    printf("\n");
                }

                fd = f_opendir(currenttokens[index]);

                if (fd == FAILURE) {
                    fd = f_open(currenttokens[index], "r");
                    if (fd == FAILURE) {
                        //TODO error handling
                    } else {
                        if (mode_l) {
                            f_stat(fd, &stats);
                            //TODO long list format
                            printf("%lld%s\n", stats.size, currenttokens[index]);
                        } else {
                            printf("%s\n", currenttokens[index]);
                        }
                        f_close(fd);
                    }

                } else {
                    ls_directory(fd, mode_F, mode_l);
                    f_closedir(fd);
                }

                first_argument = false;
            }
        }
    }
}

void my_chmod() {

}

void my_mkdir() {

}

void my_rmdir() {

}

void my_cd() {
    if (length > 2) {
        fprintf(stderr, "cd: too many arguments\n");
    } else if (length == 2){
        if (set_wd(currenttokens[1]) == FAILURE) {
            //TODO: error handling
        }
    }
}

void my_pwd() {
    printf("%s\n", wd);
}

void my_cat() {

}

void my_more() {

}

void my_rm() {

}

void my_mount() {

}

void my_unmount() {

}
