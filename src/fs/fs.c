#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>
#include "error.h"
#include "fs.h"
#include "../shell/mysh.h"

ERRNO error;
char* wd;
static vnode_t* vnodes; // root of vnode tree
static FILE* disks[MAX_DISKS]; // disks mounted
static sb_t* superblocks[MAX_DISKS]; // superblock for each disk
static file_t* ft[MAX_OPENFILE]; // open file table

int f_open(const char* pathname, const char* mode) {
    char** path;
    int length = split_path(pathname, &path);
    if (length == 0) { // `/`
        error = TARGET_EXISTS;
        return FAILURE;
    }
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; i++){
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            return FAILURE;
        }
    }

    vnode_t* vnode = get_vnode(parentdir, path[length - 1]);
    if (vnode && vnode->type == DIR) {
        // if entry exists but is a directory, return failure
        error = NOT_FILE;
        return FAILURE;
    }

    long position;
    f_mode fmode;
    if (mode[0] == 'r') {
        if (!vnode) {
            // "r" mode and file doesn't exist, return failure
            error = INVALID_PATH;
            return FAILURE;
        }
        if (!has_permission(vnode, 'r')) {
            error = PERM_DENIED;
            return FAILURE;
        }

        position = 0;
        fmode = RDONLY;
    } else {
        if (!vnode) {
            // "w" or "a" mode and file doesn't exist, create new one
            if (!has_permission(parentdir, 'w')) {
                error = PERM_DENIED;
                return FAILURE;
            }
            vnode = create_file(parentdir, path[length - 1], F, "rw--");
            if (!vnode) {
                error = DISK_FULL;
                return FAILURE;
            }
        } else {
            // file exists
            if (!has_permission(vnode, 'w')) {
                error = PERM_DENIED;
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
        return FAILURE;
    }

    free_path(path);
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

    file->position = endposition+1;
    return endposition-file->position+1;
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

    file->position = endposition+1;
    ssize_t ret = endposition-file->position+1;

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
    char** path;
    int length = split_path(pathname, &path);
    vnode_t* file = tracedown(path,length);
    if (file == NULL)
        return FAILURE;
    fetch_inode(file, inode);
    return SUCCESS;
}

int f_remove(const char* pathname) {

    char** path;
    int length = split_path(pathname, &path);
    vnode_t* vnode = tracedown(path,length);
    if (vnode == NULL)
        return FAILURE;
    inode_t inode;
    fetch_inode(vnode,&inode);
    if (inode.uid != user_id) {
        error = PERM_DENIED;
        return FAILURE;
    }

    if (vnode->type == DIR){
        return FAILURE;
    }
    //free file table entry
    for (int i = 0; i<MAX_OPENFILE; i++){
        if (ft[i]->vnode == vnode){
            ft[i] = NULL;
        }
    }
    //free blocks
    cleandata(vnode);
    //free inode
    free_inode(vnode);
    //free vnode
    free_vnode(vnode);

    return SUCCESS;
}

int f_opendir(const char* pathname) {
    char** path;
    int length = split_path(pathname, &path);
    vnode_t* vnode = vnodes;
    for (int i = 0; i < length; i++){
        vnode = get_vnode(vnode, path[i]);
        if (!vnode) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (vnode->type != DIR) {
            error = NOT_DIR;
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
        return FAILURE;
    }

    // add to open file table
    file_t* dir = malloc(sizeof(file_t));
    dir->position = 2; // go past `.` and `..`
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

    free_path(path);
    return fd;
}

int f_readdir(int fd, char** filename, inode_t* inode) {
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
    if (readdir(file->vnode, file->position++, &dirent, inode) == FAILURE)
        return F_EOF;

    if (dirent.type != EMPTY) {
        *filename = dirent.name;
        inode->size = inode->dir_size;
    } else {
        inode->type = EMPTY;
    }
    return SUCCESS;
}

int f_closedir(int fd) {
    return f_close(fd);
}

int f_mkdir(const char* pathname, char* mode, bool login) {
    char** path;
    int length = split_path(pathname, &path);
    if (length == 0) { // `/`
        error = TARGET_EXISTS;
        return FAILURE;
    }
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; i++){
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            return FAILURE;
        }
    }

    if (!login && !has_permission(parentdir, 'w')) {
        error = PERM_DENIED;
        return FAILURE;
    }

    if (get_vnode(parentdir, path[length - 1])) {
        error = TARGET_EXISTS;
        return FAILURE;
    }

    vnode_t* vnode = create_file(parentdir, path[length - 1], DIR, mode);
    if (!vnode) {
        error = DISK_FULL;
        return FAILURE;
    }

    free_path(path);
    return SUCCESS;
}

int f_rmdir(const char* pathname) {
    char** path;
    int length = split_path(pathname, &path);
    vnode_t* vnode = vnodes;
    for (int i = 0; i < length; i++){
        vnode = get_vnode(vnode, path[i]);
        if (!vnode) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (vnode->type != DIR) {
            error = NOT_DIR;
            return FAILURE;
        }
    }

    if (!has_permission(vnode, 'w')) {
        error = PERM_DENIED;
        return FAILURE;
    }
    //free blocks
    cleandata(vnode);
    //free inode
    free_inode(vnode);
    //free vnode
    free_vnode(vnode);
    return SUCCESS;
}

int f_mount(const char* source, const char* target) {
    // check target mount point
    char** path;
    int length = split_path(target, &path);
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length - 1; ++i) {
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            return FAILURE;
        }
    }
    if (length > 0 && get_vnode(parentdir, path[length - 1])) {
        error = TARGET_EXISTS;
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
        return FAILURE;
    }

    FILE* disk = fopen(source, "r+");
    if (!disk) {
        error = INVALID_SOURCE;
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
    if (length == 0) {
        // mount at `/` (for the first disk)
        strcpy(mountpoint->name, "/");
        mountpoint->parent = NULL;
        mountpoint->children = NULL;
        mountpoint->next = mountpoint;
        mountpoint->prev = mountpoint;
        vnodes = mountpoint;
    } else {
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

    free_path(path);
    return SUCCESS;
}

int f_umount(const char* target) {
    // check target mount point
    char** path;
    int length = split_path(target, &path);
    vnode_t* mountpoint = vnodes;
    for (int i = 0; i < length; ++i) {
        mountpoint = get_vnode(mountpoint, path[i]);
        if (!mountpoint) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (mountpoint->type != DIR) {
            error = NOT_DIR;
            return FAILURE;
        }
    }
    free_path(path);
    free_vnode(mountpoint);
    return SUCCESS;
}

int f_chmod(const char* pathname, char* mode){
    char** path;
    int length = split_path(pathname, &path);
    vnode_t* vnode = tracedown(path,length);
    if (vnode == NULL)
        return FAILURE;
    inode_t inode;
    fetch_inode(vnode, &inode);
    strcpy(inode.permission,mode);
    update_inode(vnode,&inode);
    return SUCCESS;
}

void init() {
    vnodes = NULL;
    for (int i = 0; i < MAX_DISKS; ++i) {
        disks[i] = NULL;
        superblocks[i] = NULL;
    }
    wd = NULL;
}

void term() {
    free_vnode(vnodes);
    for (int i = 0; i < MAX_DISKS; ++i) {
        if (disks[i]){
            fclose(disks[i]);
            free(superblocks[i]);
        }
    }
    free(wd);
}

void free_vnode(vnode_t* vnode) {
    char* rootname = "/";
    if (strcmp(vnode->name,rootname)==0){
        disks[vnode->disk] = NULL;
        superblocks[vnode->disk] = NULL;
    }

    while (vnode->children != NULL){
        free_vnode(vnode->children);
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
    char** path;
    int length = split_path(pathname, &path);
    vnode_t* dest = vnodes;
    for (int i = 0; i < length; ++i) {
        dest = get_vnode(dest, path[i]);
        if (!dest) {
            error = INVALID_PATH;
            return FAILURE;
        }
        if (dest->type != DIR) {
            error = NOT_DIR;
            return FAILURE;
        }
    }
    free_path(path);

    if (!wd || strlen(wd) < strlen(dest->name))
        wd = realloc(wd, strlen(dest->name) + 1);
    strcpy(wd, dest->name);
    return SUCCESS;
}

int split_path(const char* pathname, char*** tokens) {
    char* name;
    char* delim = "/";
    int count = 0;
    char buf[strlen(wd) + strlen(pathname) + 2];
    if (pathname[0] == '/') {
        strcpy(buf, pathname);
    } else {
        strcpy(buf, wd);
        buf[strlen(wd)] = '/';
        strcat(buf, pathname);
    }
    if ((name = strtok(buf, delim)) == NULL){
        return count;
    }
    *tokens = malloc(sizeof(char*));
    **tokens = malloc(strlen(name) + 1);
    strcpy(**tokens, name);
    count += 1;
    while((name = strtok(NULL, delim)) != NULL){
        count += 1;
        *tokens = realloc(*tokens,sizeof(char*)*count);
        *((*tokens) + count - 1) = malloc(strlen(name) + 1);
        strcpy(*((*tokens) + count - 1), name);
    }
    return count;
}

static void free_path(char** path) {

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
        if (dirent.type != EMPTY && strcmp(dirent.name, filename) != 0) {
            n++;
            continue;
        }

        // found match in disk, create new vnode
        vnode_t* newchild = malloc(sizeof(vnode_t));
        newchild->inode = dirent.inode;
        newchild->disk = parentdir->disk;
        strcpy(newchild->name, dirent.name);
        newchild->type = dirent.type;
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

    if (n >= dir_inode.size)
        return FAILURE;

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
    FILE* fs = disks[parent->disk];
    inode_t* inode = malloc(sizeof(inode_t));
    fetch_inode(parent,inode);

    int end = 0;
    long long order = (inode->size)/2+1;
    long long address = get_block_address(parent, order);

    fetch_inode(parent,inode);
    inode->dir_size++;
    inode->size++;

    dirent_t* entry = malloc(sizeof(dirent_t));
    if ((entry->inode = get_inode(parent->disk))==FAILURE){
        return NULL;
    }
    strcpy(entry->name,filename);
    entry->type = type;
    fseek(fs,address+sizeof(dirent_t)*((inode->size-1)%2),SEEK_SET);
    fwrite(entry,sizeof(dirent_t),1,fs);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    inode->mtime = tv.tv_sec;
    update_inode(parent,inode);

    // initialize children
    vnode_t* children = get_vnode(parent, filename);
    fetch_inode(children,inode);
    inode->type = type;
    strcpy(inode->permission,mode);
    inode->uid = user_id;
    inode->size = inode->dir_size = 0;
    inode->mtime = tv.tv_sec;
    update_inode(children, inode);

    free(inode);
    free(entry);
    return children;
}

static void cleandata(vnode_t* vnode){
    inode_t inode;
    fetch_inode(vnode,&inode);
    long long blocknum = inode.type == F ? (inode.size-1)/BLOCKSIZE+1 : (inode.size-1)/2+1;
    if (inode.size = 0){
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

    fseek(disk,address,SEEK_SET);
    inode_t inode;
    fetch_inode(vnode,&inode);
    inode.size = 0;
    inode.next_inode = sb->free_inode;
    fwrite(&inode,sizeof(inode_t),1,disk);
    sb->free_inode = address;
    fseek(disk,BOOTSIZE,SEEK_SET);
    fwrite(sb,sizeof(sb_t),1,disk);

    //update parent
    if (vnode->parent != NULL && vnode->disk==vnode->parent->disk) {
        dirent_t entry;
        inode_t new;
        int count = 0;
        while (readdir(vnode->parent, count, &entry, &new) == SUCCESS) {
            if (strcmp(entry.name, vnode->name) == 0) {
                long long block_number = count/2+1;
                long long address = get_block_address(vnode->parent,block_number);
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

    long long address;
    long long newaddress;
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

vnode_t* tracedown(char** path,int length){
    vnode_t* parentdir = vnodes;
    for (int i = 0; i < length; i++){
        parentdir = get_vnode(parentdir, path[i]);
        if (!parentdir) {
            error = INVALID_PATH;
            return NULL;
        }
        if (parentdir->type != DIR) {
            error = NOT_DIR;
            return NULL;
        }
    }
    return parentdir;
}
