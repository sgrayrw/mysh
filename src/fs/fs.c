#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

ERRNO error;
char *wd, *prompt_path;
static vnode_t* vnodes; // root of vnode tree
static FILE* disks[MAX_DISKS]; // disks mounted
static sb_t* superblocks[MAX_DISKS]; // superblock for each disk
static file_t* ft[MAX_OPENFILE]; // open file table
const char* const user_table1[] = {"", SUPERUSER, USER};

int f_open(const char* pathname, const char* mode) {
    int length;
    char** path = split_path(pathname, &length);
    if (length == 0) { // `/`
        error = TARGET_EXISTS;
        free_path(path, length);
        return FAILURE;
    }
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; i++){
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            free_path(path, length);
            return FAILURE;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            free_path(path, length);
            return FAILURE;
        }
    }

    vnode_t* vnode = get_vnode(parentdir, path[length - 1]);
    if (vnode && vnode->type == DIR) {
        // if entry exists but is a directory, return failure
        error = NOT_FILE;
        free_path(path, length);
        return FAILURE;
    }

    long position;
    f_mode fmode;
    if (mode[0] == 'r') {
        if (!vnode) {
            // "r" mode and file doesn't exist, return failure
            error = INVALID_PATH;
            free_path(path, length);
            return FAILURE;
        }
        if (!has_permission(vnode, 'r')) {
            error = PERM_DENIED;
            free_path(path, length);
            return FAILURE;
        }

        position = 0;
        fmode = RDONLY;
    } else {
        if (!vnode) {
            // "w" or "a" mode and file doesn't exist, create new one
            if (!has_permission(parentdir, 'w')) {
                error = PERM_DENIED;
                free_path(path, length);
                return FAILURE;
            }
            vnode = create_file(parentdir, path[length - 1], F, "rw--");
            if (!vnode) {
                error = DISK_FULL;
                free_path(path, length);
                return FAILURE;
            }
        } else {
            // file exists
            if (!has_permission(vnode, 'w')) {
                error = PERM_DENIED;
                free_path(path, length);
                return FAILURE;
            }
            if (mode[0] == 'w') {
                // with "w" mode, if file already exists, truncate file
                cleandata(vnode);
            }
        }

        if (mode[0] == 'w')
            position = 0;
        else { // mode "a"
            inode_t inode;
            fetch_inode(vnode, &inode);
            position = inode.size;
        }

        fmode = WRONLY;
    }

    // add to open file table
    file_t* file = malloc(sizeof(file_t));
    file->position = position;
    file->vnode = vnode;
    file->mode = fmode;
    int fd = -1;
    for (int i = 0; i < MAX_OPENFILE; ++i) {
        if (!ft[i]) {
            ft[i] = file;
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        error = FT_EXCEEDED;
        free_path(path, length);
        return FAILURE;
    }

    free_path(path, length);
    return fd;
}

int f_close(int fd) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd]==NULL) {
        error = INVALID_FD;
        return FAILURE;
    }
    free(ft[fd]);
    ft[fd] = NULL;
    return SUCCESS;
}

ssize_t f_read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    file_t* file = ft[fd];
    if (file->mode == WRONLY) {
        error = MODE_ERR;
        return FAILURE;
    }

    FILE* disk = disks[file->vnode->disk];
    inode_t inode;
    fetch_inode(file->vnode, &inode);
    if (file->position >= inode.size) {
        error = F_EOF;
        return FAILURE;
    }

    long long startbl = file->position/BLOCKSIZE+1;
    long long endposition = file->position+count > inode.size ? inode.size-1 : file->position+count-1;
    long long endbl = endposition/BLOCKSIZE+1;
    long long address;
    long long preoffset;
    long long postoffset;
    for (long long order = startbl; order <= endbl; order++){
        address = get_block_address(file->vnode, order);
        preoffset = order == startbl ? file->position % BLOCKSIZE : 0;
        postoffset = order == endbl ? BLOCKSIZE-(endposition%BLOCKSIZE+1) :0;
        fseek(disk,address+preoffset,SEEK_SET);
        fread(buf,BLOCKSIZE-preoffset-postoffset,1,disk);
        buf += BLOCKSIZE-preoffset-postoffset;
    }

    size_t readsize = endposition-file->position+1;
    file->position = endposition+1;
    return readsize;
}

ssize_t f_write(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    file_t* file = ft[fd];
    if (file->mode == RDONLY) {
        error = MODE_ERR;
        return FAILURE;
    }

    FILE* disk = disks[file->vnode->disk];
    inode_t inode;
    fetch_inode(file->vnode, &inode);
    size_t padding = file->position - inode.size;
    if (padding > 0) {
        // fill in NUL bytes
        void* tmp = malloc(count);
        memcpy(tmp, buf, count);
        buf = realloc(buf, padding + count);
        memset(buf, '\0', padding);
        memcpy(buf + padding, tmp, count);
        file->position = inode.size;
        count += padding;
    }

    long long startbl = file->position/BLOCKSIZE+1;
    long long endposition = file->position+count-1;
    long long endbl = endposition/BLOCKSIZE+1;
    long long address;
    long long preoffset;
    long long postoffset;
    long long size = inode.size;
    for (long long order = startbl; order <= endbl; order++){
        if ((address = get_block_address(file->vnode, order))==FAILURE){
            fetch_inode(file->vnode,&inode);
            inode.size = size;
            update_inode(file->vnode,&inode);
            error = DISK_FULL;
            return FAILURE;
        }

        preoffset = order == startbl ? file->position % BLOCKSIZE : 0;
        postoffset = order == endbl ? BLOCKSIZE-(endposition%BLOCKSIZE+1) : 0;
        fseek(disk,address+preoffset,SEEK_SET);
        fwrite(buf,BLOCKSIZE-preoffset-postoffset,1,disk);
        buf += BLOCKSIZE-preoffset-postoffset;
        size+=BLOCKSIZE-preoffset-postoffset;
    }

    ssize_t ret = endposition-file->position+1;
    file->position = endposition+1;

    fetch_inode(file->vnode,&inode);
    inode.size = size;
    // update time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    inode.mtime = tv.tv_sec;
    update_inode(file->vnode, &inode);
    return ret;
}

int f_seek(int fd, long offset, int whence) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    file_t* file = ft[fd];
    if (whence == SEEK_SET){
        file->position = offset;
    }else if (whence == SEEK_CUR){
        file->position += offset;
    }else if (whence == SEEK_END){
        inode_t inode;
        fetch_inode(file->vnode, &inode);
        file->position = inode.size + offset;
    }
    return SUCCESS;
}

int f_rewind(int fd) {
    return f_seek(fd, 0, SEEK_SET);
}

int f_stat(const char* pathname, inode_t* inode) {
    int length;
    char** path = split_path(pathname, &length);
    vnode_t* vnode = tracedown(path,length);
    if (vnode == NULL) {
        free_path(path, length);
        return FAILURE;
    }
    fetch_inode(vnode, inode);
    inode->dir_size += vnode->n_mounts;
    free_path(path,length);
    return SUCCESS;
}

int f_remove(const char* pathname) {
    int length;
    char** path = split_path(pathname, &length);
    vnode_t* vnode = tracedown(path,length);
    if (vnode == NULL) {
        free_path(path,length);
        return FAILURE;
    }
    inode_t inode;
    fetch_inode(vnode,&inode);
    if (inode.uid != user_id) {
        error = PERM_DENIED;
        free_path(path,length);
        return FAILURE;
    }

    if (vnode->type != F){
        error = NOT_FILE;
        free_path(path,length);
        return FAILURE;
    }
    //free file table entry
    for (int i = 0; i<MAX_OPENFILE; i++){
        if (ft[i] != NULL && ft[i]->vnode == vnode){
            ft[i] = NULL;
        }
    }
    //free blocks
    cleandata(vnode);
    //free inode
    free_inode(vnode);
    //free vnode
    free_vnode(vnode);

    free_path(path,length);
    return SUCCESS;
}

int f_opendir(const char* pathname) {
    int length;
    char** path = split_path(pathname, &length);
    vnode_t* vnode = vnodes;
    for (int i = 0; i < length; i++){
        vnode = get_vnode(vnode, path[i]);
        if (!vnode) {
            error = INVALID_PATH;
            free_path(path,length);
            return FAILURE;
        }
        if (vnode->type != DIR) {
            error = NOT_DIR;
            free_path(path,length);
            return FAILURE;
        }
    }

    f_mode fmode;
    if (has_permission(vnode, 'r') && has_permission(vnode, 'w'))
        fmode = RDWR;
    else if (has_permission(vnode, 'r'))
        fmode = RDONLY;
    else if (has_permission(vnode, 'w'))
        fmode = WRONLY;
    else {
        error = PERM_DENIED;
        free_path(path,length);
        return FAILURE;
    }

    // add to open file table
    file_t* dir = malloc(sizeof(file_t));
    dir->position = 0; // go past `.` and `..`
    dir->vnode = vnode;
    dir->mode = fmode;
    int fd = -1;
    for (int i = 0; i < MAX_OPENFILE; ++i) {
        if (!ft[i]) {
            ft[i] = dir;
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        error = FT_EXCEEDED;
        return FAILURE;
    }

    free_path(path, length);
    return fd;
}

int f_readdir(int fd, char* filename, inode_t* inode) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    file_t* file = ft[fd];
    if (file->mode == WRONLY) {
        error = PERM_DENIED;
        return FAILURE;
    }

    dirent_t dirent;
    if (readdir(file->vnode, file->position, &dirent, inode) == FAILURE) {
        // mountpoints
        inode_t dir_inode;
        fetch_inode(file->vnode, &dir_inode);
        int n = file->position - dir_inode.size;
        if (n >= file->vnode->n_mounts) {
            error = F_EOF;
            return FAILURE;
        }

        vnode_t* child = file->vnode->children;
        while (true) {
            if (child->inode == superblocks[child->disk]->root_inode) {
                n--;
                if (n < 0)
                    break;
            }
            child = child->next;
        }
        fetch_inode(child, inode);
        strcpy(filename, child->name);
        inode->dir_size += child->n_mounts;
        if (child->type == DIR)
            inode->size = inode->dir_size;
        file->position++;
        return SUCCESS;
    }

    if (dirent.type != EMPTY) {
        strcpy(filename, dirent.name);
        vnode_t* vnode = get_vnode(file->vnode, dirent.name);
        inode->dir_size += vnode->n_mounts;
        if (dirent.type == DIR)
            inode->size = inode->dir_size;
    } else {
        inode->type = EMPTY;
    }

    file->position++;
    return SUCCESS;
}

int f_closedir(int fd) {
    return f_close(fd);
}

int f_mkdir(const char* pathname, char* mode, bool login) {
    int length;
    char** path = split_path(pathname, &length);
    if (length == 0) { // `/`
        error = TARGET_EXISTS;
        free_path(path,length);
        return FAILURE;
    }
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; i++){
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            free_path(path,length);
            return FAILURE;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            free_path(path,length);
            return FAILURE;
        }
    }

    if (!login && !has_permission(parentdir, 'w')) {
        error = PERM_DENIED;
        free_path(path,length);
        return FAILURE;
    }

    if (get_vnode(parentdir, path[length - 1])) {
        error = TARGET_EXISTS;
        free_path(path,length);
        return FAILURE;
    }

    vnode_t* vnode = create_file(parentdir, path[length - 1], DIR, mode);
    if (!vnode) {
        error = DISK_FULL;
        free_path(path,length);
        return FAILURE;
    }

    //add . and  ..
//    FILE* fs = disks[vnode->disk];
//    inode_t inode;
//    fetch_inode(vnode,&inode);
//
//    long long order = 1;
//    long long address = get_block_address(vnode, order);
//
//    fetch_inode(vnode,&inode);
//    inode.dir_size+=2;
//    inode.size+=2;
//
//    dirent_t entry_self;
//    entry_self.inode = vnode->inode;
//    entry_self.type = DIR;
//    char* selfname = ".";
//    strcpy(entry_self.name,selfname);
//
//    dirent_t entry_parent;
//    entry_parent.inode = vnode->parent->inode;
//    entry_parent.type = DIR;
//    char* parentname = "..";
//    strcpy(entry_parent.name,parentname);
//
//    fseek(fs,address,SEEK_SET);
//    fwrite(&entry_self,sizeof(dirent_t),1,fs);
//    fwrite(&entry_parent,sizeof(dirent_t),1,fs);
//
//    struct timeval tv;
//    gettimeofday(&tv, NULL);
//    inode.mtime = tv.tv_sec;
//    update_inode(vnode,&inode);


    free_path(path, length);
    return SUCCESS;
}

int f_rmdir(const char* pathname) {
    int length;
    char** path = split_path(pathname, &length);
    vnode_t* vnode = vnodes;
    for (int i = 0; i < length; i++){
        vnode = get_vnode(vnode, path[i]);
        if (!vnode) {
            error = INVALID_PATH;
            free_path(path,length);
            return FAILURE;
        }
        if (vnode->type != DIR) {
            error = NOT_DIR;
            free_path(path,length);
            return FAILURE;
        }
    }

    inode_t inode;
    fetch_inode(vnode,&inode);
    if (inode.uid != user_id) {
        error = PERM_DENIED;
        free_path(path,length);
        return FAILURE;
    }

    if (vnode->type != DIR){
        error = NOT_DIR;
        free_path(path,length);
        return FAILURE;
    }

    //free file table entry
    for (int i = 0; i<MAX_OPENFILE; i++){
        if (ft[i] != NULL && ft[i]->vnode == vnode){
            ft[i] = NULL;
        }
    }
    //free blocks
    cleandata(vnode);
    //free inode
    free_inode(vnode);
    //free vnode
    free_vnode(vnode);

    free_path(path,length);
    return SUCCESS;
}

int f_mount(const char* source, const char* target) {
    // check target mount point
    int length;
    char** path = split_path(target, &length);
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; ++i) {
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            free_path(path,length);
            return FAILURE;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            free_path(path,length);
            return FAILURE;
        }
    }
    if (length > 0 && get_vnode(parentdir, path[length - 1])) {
        error = TARGET_EXISTS;
        free_path(path,length);
        return FAILURE;
    }

    // load disk
    int n_disk = -1;
    for (int i = 0; i < MAX_DISKS; ++i) {
        if (!disks[i]) {
            n_disk = i;
            break;
        }
    }
    if (n_disk == -1) {
        error = DISKS_EXCEEDED;
        free_path(path,length);
        return FAILURE;
    }

    FILE* disk = fopen(source, "r+");
    if (!disk) {
        error = INVALID_SOURCE;
        free_path(path,length);
        return FAILURE;
    }
    disks[n_disk] = disk;
    sb_t* sb = malloc(sizeof(sb_t));
    fseek(disk, BOOTSIZE, SEEK_SET);
    fread(sb, sizeof(sb_t), 1, disk);
    superblocks[n_disk] = sb;

    // add vnode corresponding to the mount point
    vnode_t* mountpoint = malloc(sizeof(vnode_t));
    mountpoint->inode = sb->root_inode;
    mountpoint->disk = n_disk;
    mountpoint->type = DIR;
    mountpoint->n_mounts = 0;
    if (length == 0) {
        // mount at `/` (for the first disk)
        strcpy(mountpoint->name, "/");
        mountpoint->parent = NULL;
        mountpoint->children = NULL;
        mountpoint->next = mountpoint;
        mountpoint->prev = mountpoint;
        vnodes = mountpoint;
    } else {
        parentdir->n_mounts++;
        strcpy(mountpoint->name, path[length - 1]);
        mountpoint->parent = parentdir;
        mountpoint->children = NULL;
        if (!parentdir->children) {
            parentdir->children = mountpoint;
            mountpoint->next = mountpoint;
            mountpoint->prev = mountpoint;
        } else {
            mountpoint->next = parentdir->children->next;
            mountpoint->next->prev = mountpoint;
            mountpoint->prev = parentdir->children;
            mountpoint->prev->next = mountpoint;
        }
    }

    free_path(path, length);
    return SUCCESS;
}

int f_umount(const char* target) {
    // check target mount point
    int length;
    char** path = split_path(target, &length);
    vnode_t* mountpoint = vnodes;
    for (int i = 0; i < length; ++i) {
        mountpoint = get_vnode(mountpoint, path[i]);
        if (!mountpoint) {
            error = INVALID_PATH;
            free_path(path,length);
            return FAILURE;
        }
        if (mountpoint->type != DIR) {
            error = NOT_DIR;
            free_path(path,length);
            return FAILURE;
        }
    }

    if (mountpoint->parent)
        mountpoint->parent->n_mounts--;

    free_path(path, length);
    free_vnode(mountpoint);
    return SUCCESS;
}

int f_chmod(const char* pathname, char* mode){
    int length;
    char** path = split_path(pathname, &length);
    vnode_t* vnode = tracedown(path,length);
    if (vnode == NULL) {
        free_path(path,length);
        return FAILURE;
    }
    inode_t inode;
    fetch_inode(vnode, &inode);
    strcpy(inode.permission,mode);
    update_inode(vnode,&inode);
    free_path(path,length);
    return SUCCESS;
}

void init_fs() {
    vnodes = NULL;
    for (int i = 0; i < MAX_DISKS; ++i) {
        disks[i] = NULL;
        superblocks[i] = NULL;
    }
    wd = malloc(sizeof(char) * 2);
    strcpy(wd, "/");
    prompt_path = malloc(sizeof(char) * 2);
    strcpy(prompt_path, "/");
}

void term_fs() {
    free_vnode(vnodes);
    free(wd);
    free(prompt_path);
}

void free_vnode(vnode_t* vnode) {
    while (vnode->children != NULL)
        free_vnode(vnode->children);

    // if it's a mount point, close disk and free superblock
    if (vnode->inode == superblocks[vnode->disk]->root_inode) {
        fclose(disks[vnode->disk]);
        disks[vnode->disk] = NULL;
        free(superblocks[vnode->disk]);
        superblocks[vnode->disk] = NULL;
    }

    if (vnode->parent == NULL){
        free(vnode);
        return;
    }

    if (vnode->next == vnode){
        vnode->parent->children = NULL;
    }else{
        vnode->parent->children = vnode->next;
        vnode->next->prev = vnode->prev;
        vnode->prev->next = vnode->next;
    }
    free(vnode);
}

void dump() {
    if (vnodes)
        dump_vnode(vnodes, 1);
}

void dump_vnode(vnode_t* vnode, int depth) {
    printf("%s\n", vnode->name);
    vnode_t* child = vnode->children;
    if (child) {
        do {
            for (int i = 0; i < depth; ++i)
                printf("|-- ");
            dump_vnode(child, depth + 1);
            child = child->next;
        } while (strcmp(child->name, vnode->children->name) != 0);
    }
}

int set_wd(const char* pathname) {
    // check target mount point
    int length;
    char** path = split_path(pathname, &length);
    vnode_t* dest = vnodes;
    for (int i = 0; i < length; ++i) {
        dest = get_vnode(dest, path[i]);
        if (!dest) {
            error = INVALID_PATH;
            free_path(path,length);
            return FAILURE;
        }
        if (dest->type != DIR) {
            error = NOT_DIR;
            free_path(path,length);
            return FAILURE;
        }
    }

    if (wd) free(wd);
    if (length == 0) {
        wd = malloc(sizeof(char) + 1);
        strcpy(wd, "/");
        prompt_path = realloc(prompt_path, sizeof(char) * 2);
        strcpy(prompt_path, "/");
    } else if (length == 1 && strcmp(path[0], user_table1[user_id]) == 0) {
        prompt_path = realloc(prompt_path, sizeof(char) * 2);
        strcpy(prompt_path, "~");
    } else {
        prompt_path = realloc(prompt_path, sizeof(char) * (strlen(path[length - 1]) + 1));
        strcpy(prompt_path, path[length - 1]);
    }
    int wd_length = 0;
    for (int j = 0; j < length; j++) {
        wd_length += (int) (strlen(*(path + j)) + 1);
        if (j == 0) {
            wd = malloc(sizeof(char) * (wd_length + 1));
            strcpy(wd, "/");
            strcat(wd, *(path + j));
        } else {
            wd = realloc(wd, wd_length + 1);
            strcat(wd, "/");
            strcat(wd, *(path + j));
        }
    }
    free_path(path, length);
    return SUCCESS;
}

char** split_path(const char* pathname, int* length) {
    char* name;
    char* delim = "/";
    char *buf = malloc(sizeof(char) * (strlen(wd) + strlen(pathname) + 2));
    if (pathname[0] == '/') {
        strcpy(buf, pathname);
    } else {
        strcpy(buf, wd);
        if (strcmp(wd, "/") != 0) {
            buf[strlen(wd)] = '/';
            buf[strlen(wd) + 1] = '\0';
        }
        strcat(buf, pathname);
    }
    *length = 0;
    if ((name = strtok(buf, delim)) == NULL){
        free(buf);
        return NULL;
    }
    char **tokens = malloc(sizeof(char*));
    *tokens = NULL;
    char *token;
    if (strcmp(name, "..") != 0 && strcmp(name, ".") != 0) {
        token = malloc(strlen(name) + 1);
        strcpy(token, name);
        *length += 1;
        tokens = realloc(tokens, sizeof(char *) * (*length + 1));
        tokens[*length - 1] = token;
        tokens[*length] = NULL;
    }
    while((name = strtok(NULL, delim)) != NULL){
        if (strcmp(name, ".") == 0) {
            continue;
        } else if (strcmp(name, "..") == 0) {
            if (*length == 0) {
                continue;
            } else {
                *length -= 1;
                free(tokens[*length]);
                tokens[*length] = NULL;
                tokens = realloc(tokens, sizeof(char *) * (*length + 1));
            }
        } else {
            token = malloc(strlen(name) + 1);
            strcpy(token, name);
            *length += 1;
            tokens = realloc(tokens, sizeof(char *) * (*length + 1));
            tokens[*length - 1] = token;
            tokens[*length] = NULL;
        }
    }
    free(buf);
    return tokens;
}

static void free_path(char** path, int length) {
    for (int i = 0; i < length; ++i)
        free(path[i]);
    free(path);
}

vnode_t* get_vnode(vnode_t* parentdir, char* filename) {
    // `.` and `..`
    if (strcmp(filename, ".") == 0)
        return parentdir;
    else if (strcmp(filename, "..") == 0)
        return parentdir->parent;

    // find from existing vnodes
    vnode_t* child = parentdir->children;
    if (child) {
        do {
            if (strcmp(child->name, filename) == 0)
                return child;
            child = child->next;
        } while (strcmp(child->name, parentdir->children->name) != 0);
    }
    // load from disk
    dirent_t dirent;
    inode_t inode;
    int n = 0;
    while (readdir(parentdir, n, &dirent, &inode) != FAILURE) {
        if (dirent.type == EMPTY || strcmp(dirent.name, filename) != 0) {
            n++;
            continue;
        }

        // found match in disk, create new vnode
        vnode_t* newchild = malloc(sizeof(vnode_t));
        newchild->inode = dirent.inode;
        newchild->disk = parentdir->disk;
        strcpy(newchild->name, dirent.name);
        newchild->type = dirent.type;
        newchild->n_mounts = 0;
        newchild->parent = parentdir;
        newchild->children = NULL;

        // add to children list of parentdir
        if (!parentdir->children) {
            parentdir->children = newchild;
            newchild->next = newchild;
            newchild->prev = newchild;
        } else {
            newchild->next = parentdir->children->next;
            newchild->next->prev = newchild;
            newchild->prev = parentdir->children;
            newchild->prev->next = newchild;
        }
        return newchild;
    }
    return NULL;
}

void fetch_inode(vnode_t* vnode, inode_t* inode) {
    FILE* disk = disks[vnode->disk];
    fseek(disk, vnode->inode, SEEK_SET);
    fread(inode, sizeof(inode_t), 1, disk);
}

void update_inode(vnode_t* vnode, inode_t* inode) {
    FILE* disk = disks[vnode->disk];
    fseek(disk, vnode->inode, SEEK_SET);
    fwrite(inode, sizeof(inode_t), 1, disk);
}

int readdir(vnode_t* dir, int n, dirent_t* dirent, inode_t* inode) {
    inode_t dir_inode;
    fetch_inode(dir, &dir_inode);
    if (n >= dir_inode.size) {
        return FAILURE;
    }
    FILE* disk = disks[dir->disk];
    long long block_number = n / 2 + 1;
    long long block_addr = get_block_address(dir, block_number);
    fseek(disk, block_addr + (sizeof(dirent_t) * (n % 2)), SEEK_SET);
    fread(dirent, sizeof(dirent_t), 1, disk);
    if (dirent->type != EMPTY) {
        fseek(disk, dirent->inode, SEEK_SET);
        fread(inode, sizeof(inode_t), 1, disk);
    }
    return SUCCESS;
}

long long get_block(int n_disk) {
    FILE* disk = disks[n_disk];
    sb_t* sb = superblocks[n_disk];
    if (sb->free_block == -1)
        return FAILURE;

    // get next free block
    long long free_block = sb->free_block;
    // update free block list in superblock
    fseek(disk, free_block, SEEK_SET);
    fread(&(sb->free_block), sizeof(long long), 1, disk);

    // update superblock in disk
    fseek(disk, BOOTSIZE, SEEK_SET);
    fwrite(sb, sizeof(sb_t), 1, disk);
    return free_block;
}

static long long get_inode(int n_disk){
    FILE* disk = disks[n_disk];
    sb_t* sb = superblocks[n_disk];

    if (sb->free_inode==-1){
        return FAILURE;
    }

    long long address = sb->free_inode;
    fseek(disk,address,SEEK_SET);
    inode_t* inode = malloc(sizeof(inode_t));
    fread(inode,sizeof(inode_t),1,disk);
    sb->free_inode = inode->next_inode;

    fseek(disk, BOOTSIZE, SEEK_SET);
    fwrite(sb,sizeof(sb_t),1, disk);

    free(inode);
    return address;
}

static vnode_t* create_file(vnode_t* parent, char* filename, f_type type, char* mode){
    FILE* disk = disks[parent->disk];
    inode_t* inode = malloc(sizeof(inode_t));
    fetch_inode(parent,inode);
    long long order = (inode->size)/2+1;
    long long address = get_block_address(parent, order);

    fetch_inode(parent,inode);
    inode->dir_size++;
    inode->size++;

    dirent_t* entry = calloc(1, sizeof(dirent_t));
    if ((entry->inode = get_inode(parent->disk))==FAILURE){
        return NULL;
    }
    strcpy(entry->name,filename);
    entry->type = type;
    fseek(disk,address+sizeof(dirent_t)*((inode->size-1)%2),SEEK_SET);
    fwrite(entry,sizeof(dirent_t),1,disk);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    inode->mtime = tv.tv_sec;
    update_inode(parent,inode);

    // initialize children
    strcpy(inode->permission,mode);
    inode->size = inode->dir_size = 0;
    inode->type = type;
    inode->uid = user_id;
    inode->mtime = tv.tv_sec;
    fseek(disk, entry->inode, SEEK_SET);
    fwrite(inode, sizeof(inode_t), 1, disk);
    free(inode);
    free(entry);
    vnode_t* children = get_vnode(parent, filename);
    return children;
}

static void cleandata(vnode_t* vnode){
    inode_t inode;
    fetch_inode(vnode,&inode);
    long long blocknum = inode.type == F ? (inode.size-1)/BLOCKSIZE+1 : (inode.size-1)/2+1;
    if (inode.size == 0){
        blocknum = 0;
    }
    for (int i = 1; i<= blocknum; i++){
        long long address = get_block_address(vnode,i);
        free_block(vnode->disk, address);
    }
    inode.size = 0;
    update_inode(vnode,&inode);
}

void free_block(int n_disk, long long address){
    FILE* disk = disks[n_disk];
    sb_t* sb = superblocks[n_disk];

    fseek(disk,address,SEEK_SET);
    fwrite(&(sb->free_block),sizeof(long long),1,disk);
    sb->free_block = address;
    fseek(disk,BOOTSIZE,SEEK_SET);
    fwrite(sb,sizeof(sb_t),1,disk);
}

void free_inode(vnode_t* vnode){
    FILE* disk = disks[vnode->disk];
    sb_t* sb = superblocks[vnode->disk];
    long long address = vnode->inode;

    inode_t inode;
    fetch_inode(vnode,&inode);
    inode.size = 0;
    inode.next_inode = sb->free_inode;
    update_inode(vnode,&inode);
    sb->free_inode = address;
    fseek(disk,BOOTSIZE,SEEK_SET);
    fwrite(sb,sizeof(sb_t),1,disk);

    //update parent
    if (vnode->parent != NULL && vnode->disk==vnode->parent->disk) {
        dirent_t entry;
        inode_t new;
        int count = 0;
        while (readdir(vnode->parent, count, &entry, &new) == SUCCESS) {
            if (entry.type!=EMPTY && strcmp(entry.name, vnode->name) == 0) {
                long long block_number = count/2+1;
                address = get_block_address(vnode->parent,block_number);
                entry.type = EMPTY;
                fseek(disk,address+sizeof(dirent_t)*(count%2),SEEK_SET);
                fwrite(&entry,sizeof(dirent_t),1,disk);
                fetch_inode(vnode->parent,&inode);
                inode.dir_size--;
                struct timeval tv;
                gettimeofday(&tv, NULL);
                inode.mtime = tv.tv_sec;
                update_inode(vnode->parent,&inode);
                return;
            }
            count ++;
        }
    }
}

int get_block_index(long long block_number, int* index){
    index[5] = 0;
    long long order = block_number;
    int sum[5];
    if (order<=(sum[0]=10)){
        index[0] = 0;
    }else if(order <=(sum[1]=64+10)){
        index[0] = 1;
    }else if(order <= (sum[2]=64^2+64+10)){
        index[0] = 2;
    }else if(order <= (sum[3] = 64^3+64^2+64+10)){
        index[0] = 3;
    }else if(order <= (sum[4] = 64^4+64^3+64^2+64+10)){
        index[0] = 4;
    }else{
        return FAILURE;
    }

    for (int i = 1; i<5; i++){
        if (index[0]>=i){
            order = i == 1 ? order - sum[index[0]-1] : order-index[i-1]*64^(index[0]-i+1);
            index[i] = (order-1)/64^(index[0]-i);
            if (index[i] < 0){
                index[i] = 0;
            }
            index[5] = index[i]>0 ? i : index[5];
        }else{
            index[i] = 0;
            if (index[0]==0){
                index[1] = order-1;
            }
        }
    }
    return SUCCESS;
}

long long get_block_address(vnode_t* vnode, long long block_number){
    FILE* fs = disks[vnode->disk];
    inode_t inode;
    fetch_inode(vnode, &inode);

    long long totalblock;
    if (inode.size == 0)
        totalblock = 0;
    else if (inode.type == F)
        totalblock = (inode.size - 1) / BLOCKSIZE + 1;
    else
        totalblock = (inode.size - 1) / 2 + 1;

    bool new = false;
    if (block_number>totalblock){
        new = true;
    }
    int index[6];
    if (get_block_index(block_number,index)==FAILURE){
        return FAILURE;
    }

    long long address = 0;
    long long newaddress = 0;
    if (index[0] == 1){
        if (index[5] == 0 && new){
            if ((newaddress = get_block(vnode->disk)==FAILURE)){
                return FAILURE;
            }
            inode.iblock = newaddress;
            index[5] ++;
        }
        address = inode.iblock;
    }else if (index[0] == 2){
        if (index[5] == 0 && new){
            if ((newaddress = get_block(vnode->disk)==FAILURE)){
                return FAILURE;
            }
            inode.i2block = newaddress;
            index[5] ++;
        }
        address = inode.i2block;
    }else if (index[0] == 3){
        if (index[5] == 0 && new){
            if ((newaddress = get_block(vnode->disk)==FAILURE)){
                return FAILURE;
            }
            inode.i3block = newaddress;
            index[5] ++;
        }
        address = inode.i3block;
    }else if (index[0] == 4){
        if (index[5] == 0 && new){
            if ((newaddress = get_block(vnode->disk)==FAILURE)){
                return FAILURE;
            }
            inode.i4block = newaddress;
            index[5] ++;
        }
        address = inode.i4block;
    }
    fseek(fs,vnode->inode,SEEK_SET);
    fwrite(&inode,sizeof(inode_t),1,fs);
    if (new){
        if (index[0] == 0){
            if ((newaddress=get_block(vnode->disk)) ==FAILURE){
                return FAILURE;
            }
            inode.dblocks[index[1]] = newaddress;
            fseek(fs,vnode->inode,SEEK_SET);
            fwrite(&inode,sizeof(inode_t),1,fs);
            return newaddress;
        }else{
            for (int i = 1; i<index[5] ; i++){
                fseek(fs, address+sizeof(long long)*index[i],SEEK_SET);
                fread(&address, sizeof(long long),1,fs);
            }
            fseek(fs, address+sizeof(long long)*index[index[5]],SEEK_SET);
            for (int i = index[5]; i<=index[0]; i++){
                if ((newaddress = get_block(vnode->disk)==FAILURE)){
                    return FAILURE;
                }
                fwrite(&newaddress, sizeof(long long), 1, fs);
                fseek(fs, newaddress,SEEK_SET);
            }
        }
        return newaddress;
    }else{
        if (index[0] == 0){
            return inode.dblocks[index[1]];
        }else{
            for (int i = 1; i<=index[0] ; i++){
                fseek(fs, address+sizeof(long long)*index[i],SEEK_SET);
                fread(&address, sizeof(long long),1,fs);
            }
            return address;
        }
    }
}

bool has_permission(vnode_t* vnode, char mode) {
    if (user_id == ID_SUPERUSER)
        return true;

    inode_t inode;
    fetch_inode(vnode, &inode);

    if (inode.uid == user_id)
        return (inode.permission[0] == mode || inode.permission[1] == mode);
    else
        return (inode.permission[2] == mode || inode.permission[3] == mode);
}

vnode_t* tracedown(char** path, int length){
    vnode_t* vnode = vnodes;
    for (int i = 0; i < length; i++){
        vnode = get_vnode(vnode, path[i]);
        if (!vnode) {
            error = INVALID_PATH;
            return NULL;
        }
        if (i < length - 1 && vnode->type != DIR) {
            error = NOT_DIR;
            return NULL;
        }
    }
    return vnode;
}
