#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

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

const char* const user_table[] = {"", SUPERUSER, USER};

int builtin(char** neededtokens, int argclength){
    currenttokens = neededtokens;
    length = argclength;
    if (length > 0 ){
        if (strcmp(currenttokens[0],"jobs")==0){
            my_jobs();
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
        } else if (strcmp(currenttokens[0], "mount") == 0) {
            my_mount();
            return true;
        } else if (strcmp(currenttokens[0],"ls")==0) {
            my_ls();
            return true;
        } else if (strcmp(currenttokens[0],"cd")==0) {
            my_cd();
            return true;
        } else if (strcmp(currenttokens[0],"rm")==0) {
            my_rm();
            return true;
        } else if (strcmp(currenttokens[0],"cat")==0) {
            my_cat();
            return true;
        } else if (strcmp(currenttokens[0],"pwd")==0) {
            my_pwd();
            return true;
        } else if (strcmp(currenttokens[0],"more")==0) {
            my_more();
            return true;
        } else if (strcmp(currenttokens[0],"mkdir")==0) {
            my_mkdir();
            return true;
        } else if (strcmp(currenttokens[0],"chmod")==0) {
            my_chmod();
            return true;
        } else if (strcmp(currenttokens[0],"rmdir")==0) {
            my_rmdir();
            return true;
        } else if (strcmp(currenttokens[0],"umount")==0) {
            my_umount();
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
    term();
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
            }
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

hw7 built-ins

****************************/

void error_display() {
    switch (error) {
        case TARGET_EXISTS:
            fprintf(stderr, "File exists\n");
            break;
        case INVALID_PATH:
            fprintf(stderr, "No such file or directory\n");
            break;
        case NOT_DIR:
            fprintf(stderr, "Not a directory\n");
            break;
        case PERM_DENIED:
            fprintf(stderr, "Permission denied\n");
            break;
        case DISK_FULL:
            fprintf(stderr, "Disk full\n");
            break;
        case NOT_FILE:
            fprintf(stderr, "Is a directory\n");
            break;
        case FT_EXCEEDED:
            fprintf(stderr, "Maximum number of opened files reached\n");
            break;
        default:
            fprintf(stderr, "Unknown error\n");
            break;
    }
}

void ls_directory(int fd, bool mode_F, bool mode_l) {
    char indicator, *filename;
    inode_t stats;
    bool need_to_append_newline = false;
    while (f_readdir(fd, &filename, &stats) == SUCCESS) {
        if (stats.type == EMPTY) {
            continue;
        }
        indicator = 0;
        if (mode_F && stats.type == DIR) {
            indicator = '/';
        }
        if (mode_l) {
            printf("%c%c%c%c", stats.permission[0], stats.permission[1], stats.permission[2], stats.permission[3]);
            printf("\t%s\t%lld\t%s\t%s%c\n", user_table[stats.uid], stats.size, ctime(&stats.mtime), filename, indicator);
        } else {
            printf("%s%c\t", filename, indicator);
            need_to_append_newline = true;
        }
    }
    if (need_to_append_newline) {
        printf("\n");
    }
}

void my_ls() {
    bool mode_F = false, mode_l = false, no_arguments = true;
    int index;
    for (index = 1; index < length; index++) {
        if (currenttokens[index][0] == '_' && strlen(currenttokens[index]) > 1) {
            for (int i = 1; i < (int) strlen(currenttokens[index]); i++) {
                if (currenttokens[index][i] == 'F') {
                    mode_F = true;
                } else if (currenttokens[index][i] == 'l') {
                    mode_l = true;
                } else {
                    fprintf(stderr, "ls: invalid option -- '%c'\n", currenttokens[index][i]);
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
        ls_directory(fd, mode_F, mode_l);
        f_closedir(fd);

    } else {
        bool first_argument = true;
        inode_t stats;
        for (index = 1; index < length; index++) {
            if (strlen(currenttokens[index]) == 1 || currenttokens[index][0] != '_') {

                if (!first_argument) {
                    printf("\n");
                }

                fd = f_opendir(currenttokens[index]);

                if (fd == FAILURE) {
                    if (f_stat(currenttokens[index], &stats) == FAILURE) {
                        //TODO error handling
                    } else {
                        printf("%s:\n", currenttokens[index]);
                        if (mode_l) {
                            printf("%c%c%c%c", stats.permission[0], stats.permission[1], stats.permission[2], stats.permission[3]);
                            printf("\t%s\t%lld\t%s\t%s\n", user_table[stats.uid], stats.size, ctime(&stats.mtime), currenttokens[index]);
                        } else {
                            printf("%s\n", currenttokens[index]);
                        }
                    }

                } else {
                    printf("%s:\n", currenttokens[index]);
                    ls_directory(fd, mode_F, mode_l);
                    f_closedir(fd);
                }

                first_argument = false;
            }
        }
    }
}

void my_chmod() {
    enum {Invalid, Absolute, Symbolic} mode;
    bool r_bit = false, w_bit = false;
    char permission_new[4];
    if (length < 3) {
        fprintf(stderr, "usage: chmod <permissions> <file>...\n");
    } else {

        if (currenttokens[1][0] == 'u' || currenttokens[1][0] == 'o' || currenttokens[1][0] == 'a') {
            if (currenttokens[1][1] == '+' || currenttokens[1][1] == '-' || currenttokens[1][1] == '=') {
                mode = Symbolic;
                for (int index = 2; index < strlen(currenttokens[1]); index++) {
                    if (currenttokens[1][index] == 'r') {
                        r_bit = true;
                    } else if (currenttokens[1][index] == 'w') {
                        w_bit = true;
                    } else {
                        mode = Invalid;
                        break;
                    }
                }
            } else {
                mode = Invalid;
            }

        } else {
            char *remains = NULL;
            long permission_num = strtol(currenttokens[1], &remains, 4);
            if (0 <= permission_num && permission_num <= 15 && remains == NULL) {
                mode = Absolute;
                if (permission_num >= 8) {
                    permission_new[0] = 'r';
                    permission_num -= 8;
                } else {
                    permission_new[0] = '-';
                }
                if (permission_num >= 4) {
                    permission_new[1] = 'w';
                    permission_num -= 4;
                } else {
                    permission_new[1] = '-';
                }
                if (permission_num >= 2) {
                    permission_new[2] = 'r';
                    permission_num -= 2;
                } else {
                    permission_new[2] = '-';
                }
                if (permission_num >= 1) {
                    permission_new[3] = 'w';
                } else {
                    permission_new[3] = '-';
                }
            } else {
                mode = Invalid;
            }
        }

        if (mode == Invalid) {
            fprintf(stderr, "chmod: invalid mode: '%s'\n", currenttokens[1]);
        } else {
            for (int i = 2; i < length; i++) {
                inode_t stats;
                if (f_stat(currenttokens[i], &stats) == FAILURE) {
                    //TODO error handling
                } else if (user_id != ID_SUPERUSER && stats.uid != user_id){
                    fprintf(stderr, "chmod: '%s': permission denied\n", currenttokens[i]);
                } else {
                    if (mode == Absolute) {
                        if (f_chmod(currenttokens[i], permission_new) == FAILURE) {
                            //TODO error handling
                        }
                    } else {
                        if (currenttokens[1][0] != '0') {
                            if (r_bit) {
                                if (currenttokens[1][1] == '-') {
                                    stats.permission[0] = '-';
                                } else {
                                    stats.permission[0] = 'r';
                                }
                            }
                            if (w_bit) {
                                if (currenttokens[1][1] == '-') {
                                    stats.permission[1] = '-';
                                } else {
                                    stats.permission[1] = 'w';
                                }
                            }
                        }
                        if (currenttokens[1][0] != 'u') {
                            if (r_bit) {
                                if (currenttokens[1][1] == '-') {
                                    stats.permission[2] = '-';
                                } else {
                                    stats.permission[2] = 'r';
                                }
                            }
                            if (w_bit) {
                                if (currenttokens[1][1] == '-') {
                                    stats.permission[3] = '-';
                                } else {
                                    stats.permission[3] = 'w';
                                }
                            }
                        }
                        if (f_chmod(currenttokens[i], stats.permission) == FAILURE) {
                            //TODO error handling
                        }
                    }
                }
            }
        }

    }
}

void my_mkdir() {
    if (length == 1) {
        fprintf(stderr, "usage: mkdir <directory>...\n");
    } else {
        for (int i = 1; i < length; i++) {
            if (f_mkdir(currenttokens[i], "rw--") == FAILURE) {
                fprintf(stderr, "mkdir: cannot create directory '%s': ", currenttokens[i]);
                error_display();
            }
        }
    }
}

void my_rmdir() {
    if (length == 1) {
        fprintf(stderr, "usage: rmdir <directory>...\n");
    } else {
        for (int i = 1; i < length; i++) {
            if (f_rmdir(currenttokens[i]) == FAILURE) {
                fprintf(stderr, "rmdir: failed to remove '%s': ", currenttokens[i]);
                error_display();
            }
        }
    }
}

void my_cd() {
    if (length > 2) {
        fprintf(stderr, "cd: too many arguments\n");
    } else if (length == 2){
        if (set_wd(currenttokens[1]) == FAILURE) {
            fprintf(stderr, "cd: %s: ", currenttokens[1]);
            error_display();
        }
    }
}

void my_pwd() {
    printf("%s\n", wd);
}

void cat_helper(int fd) {
    char buffer[BUFSIZE];
    ssize_t n;
    while ((n = f_read(fd, buffer, BUFSIZE)) > 0) {
        fwrite(buffer, n, 1, stdout);
    }
}

void my_cat() {

    if (length == 1) {
        cat_helper(STDIN_FILENO);
    } else {
        int fd;
        for (int i = 1; i < length; i++) {
            if (currenttokens[i][0] == '-' && currenttokens[i][1] == '\0') {
                cat_helper(STDIN_FILENO);
            }
            fd = f_open(currenttokens[i], "r");
            if (fd == FAILURE) {
                fprintf(stderr, "cat: %s: ", currenttokens[i]);
                error_display();
            } else {
                cat_helper(fd);
                f_close(fd);
            }
        }
    }
}

void my_more() {
    if (length == 1) {
        fprintf(stderr, "usage: more <file>...\n");
    } else {
        int fd, n;
        long long count;
        char buffer[BUFSIZE];
        inode_t stats;
        char *line = NULL;
        size_t t = 0;

        for (int i = 1; i < length; i++) {
            fd = f_open(currenttokens[i], "r");
            if (fd == FAILURE) {
                fprintf(stderr, "more: %s: ", currenttokens[i]);
                error_display();
            } else {
                f_stat(currenttokens[i], &stats);
                count = 0;
                while (count < stats.size) {
                    n = f_read(fd, buffer, BUFSIZE);
                    count += n;
                    fwrite(buffer, n, 1, stdout);
                    if (count < stats.size) {
                        printf("\n -- Press Enter for the next page (type q to quit): ");
                        getline(&line, &t, stdin);
                        if (strcmp(line, "q\n") == 0) {
                            if (line) free(line);
                            return;
                        }
                    }
                }
                if (i < length - 1) {
                    printf("\n -- Press Enter for the next file (type q to quit): ");
                    getline(&line, &t, stdin);
                    if (strcmp(line, "q\n") == 0) {
                        if (line) free(line);
                        return;
                    }
                }
            }
        }
        if (line) free(line);
    }
}

void my_rm() {
    if (length == 1) {
        fprintf(stderr, "usage: rm <file>...\n");
    } else {
        for (int i = 1; i < length; i++) {
            if (f_remove(currenttokens[i]) == FAILURE) {
                //TODO error handling
            }
        }
    }
}

void my_mount() {
    if (length == 1) {
        //TODO mount table
    } else if (length == 3) {
        if (f_mount(currenttokens[1], currenttokens[2]) == FAILURE) {
            switch (error) {
                case DISKS_EXCEEDED:
                    fprintf(stderr, "mount: %s: Maximum number of disk mounts reached\n", currenttokens[1]);
                    break;
                case INVALID_SOURCE:
                    fprintf(stderr, "mount: %s: Invalid disk\n", currenttokens[1]);
                    break;
                default:
                    fprintf(stderr, "mount: %s: ", currenttokens[2]);
                    error_display();
                    break;
            }
        }
    } else {
        fprintf(stderr, "usage: mount <source> <target>\n");
    }
}

void my_umount() {
    if (length == 1) {
        fprintf(stderr, "usage: umount <target>...\n");
    } else {
        for (int i = 1; i < length; i++) {
            if (f_umount(currenttokens[i]) == FAILURE) {
                //TODO error handling
            }
        }
    }
}
