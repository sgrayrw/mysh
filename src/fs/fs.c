#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "fs.h"

errno error;
char* wd;
static vnode_t* vnodes; // root of vnode tree
static FILE* disks[MAX_DISKS]; // disks mounted
static sb_t* superblocks[MAX_DISKS]; // superblock for each disk
static file_t* ft[MAX_OPENFILE]; // open file table

int f_open(const char* pathname, const char* mode) {
    char** path;
    int length = split_path(pathname, &path);
    vnode_t* current = vnodes;
    for (int i = 0; i < length-1; i++){
        char* name = *(path+i);
        if ((current = get_vnode(current, name)) == NULL){
            return FAILURE;
        }
    }
    char* name = *(path+length-1);
    if (get_vnode(current, name) == NULL){
        if (create_file(current, name)==NULL){
            return FAILURE;
        }
    }
    return SUCCESS;
}

int f_close(int fd) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd]==NULL) {
        error = INVALID_FD;
        return FAILURE;
    }
    ft[fd] = NULL;
    return SUCCESS;
}

ssize_t f_read(int fd, void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    file_t* file = ft[fd];
    inode_t inode;
    vnode_to_inode(file->vnode, &inode);
    if (file->position >= inode.size) {
        error = F_EOF;
        return FAILURE;
    }
    

}

ssize_t f_write(int fd, const void* buf, size_t count) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    // TODO: seek buf w/ NUL bytes

    long pos = ft[fd]->position;


}

int f_seek(int fd, long offset, int whence) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    file_t* file = ft[fd];
    if (whence  == SEEK_SET){
        file->position = offset;
    }else if (whence == SEEK_CUR){
        file->position += offset;
    }else if (whence == SEEK_END){
        long long address = file->vnode->inode;
        inode_t inode;
        vnode_to_inode(file->vnode, &inode);
        file->position = inode.size + offset;
    }
    return SUCCESS;
}

int f_rewind(int fd) {
    return f_seek(fd, 0, SEEK_SET);
}

int f_stat(int fd, inode_t* inode) {
    if (fd < 0 || fd >= MAX_OPENFILE || ft[fd] == NULL) {
        error = INVALID_FD;
        return FAILURE;
    }

    vnode_to_inode(ft[fd]->vnode, inode);
    return SUCCESS;
}

int f_remove(int fd) {
    file_t* file = ft[fd];
    vnode_t* vnode = file->vnode;
    FILE* disk = disks[vnode->disk];
    inode_t* inode = malloc(sizeof(inode_t));
    fseek(disk,vnode->inode,SEEK_SET);
    fread(inode,sizeof(inode_t),1,disk);
    if (inode->type == DIR){
        return FAILURE;
    }
    //free blocks

    //free inode
    free_inode(vnode->disk,vnode->inode);
    //free parent entry
    vnode_t* parent = vnode->parent;
    dirent_t* entry = malloc(sizeof(dirent_t));
    inode_t* new;
    int count = 0;
    while(readdir(parent,count,entry,inode) == SUCCESS){
        if (strcmp(entry->name,vnode->name)==0){

        }
    }

}

int f_opendir(const char* pathname) {

}

int f_readdir(int fd, char** filename, inode_t* inode) {

}

int f_closedir(int fd) {
    return f_close(fd);
}

int f_mkdir(const char* pathname, const char* mode) {

}

int f_rmdir(const char* pathname) {

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

    FILE* disk = fopen(source, "r");
    if (!disk) {
        error = INVALID_SOURCE;
        return FAILURE;
    }
    disks[n_disk] = disk;
    sb_t* sb = malloc(sizeof(sb_t));
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
    rm_vnode(mountpoint);
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
    rm_vnode(vnodes);
    for (int i = 0; i < MAX_DISKS; ++i) {
        if (disks[i]){
            fclose(disks[i]);
            free(superblocks[i]);
        }
    }
    free(wd);
}

void rm_vnode(vnode_t* vnode) {
    char rootname[] = "/";
    if (strcmp(vnode->name,rootname)==0){
        disks[vnode->disk] = NULL;
        superblocks[vnode->disk] = NULL;
    }

    while (vnode->children != NULL){
        rm_vnode(vnode->children);
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
    char buf[strlen(pathname + 1)];
    strcpy(buf, pathname);
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
        if (strcmp(dirent.name, filename) != 0) {
            n++;
            continue;
        }

        // found match in disk, create new vnode
        vnode_t* newchild = malloc(sizeof(vnode_t));
        newchild->inode = dirent.inode;
        newchild->disk = parentdir->disk;
        strcpy(newchild->name, dirent.name);
        newchild->type = inode.type;
        newchild->parent = parentdir;
        newchild->children = NULL;
        newchild->cur_entry = 0;

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

void vnode_to_inode(vnode_t* vnode, inode_t* inode) {
    FILE* disk = disks[vnode->disk];
    fseek(disk, vnode->inode, SEEK_SET);
    fread(inode, sizeof(inode_t), 1, disk);
}

int readdir(vnode_t* dir, int n, dirent_t* dirent, inode_t* inode) {
    inode_t dir_inode;
    vnode_to_inode(dir, &dir_inode);

    if (n > dir_inode.size)
        return FAILURE;

    read(dir, dirent, n * sizeof(dirent_t), sizeof(dirent_t));

    FILE* disk = disks[dir->disk];
    fseek(disk, dirent->inode, SEEK_SET);
    fread(inode, sizeof(inode_t), 1, disk);
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

vnode_t* create_file(vnode_t* parent, char* filename){
    FILE* fs = disks[parent->disk];
    inode_t* inode = malloc(sizeof(inode_t));
    fseek(fs, parent->inode,SEEK_SET);
    fread(inode,sizeof(inode_t),1,fs);

    int end = 0;
    long long order = (inode->size)/2+1;
    int index[5];
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
        return NULL;
    }

    for (int i = 1; i<5; i++){
        if (index[0]>=i){
            order = i == 1 ? order - sum[index[0]-1] : order-index[i-1]*64^(index[0]-i+1);
            index[i] = (order-1)/64^(index[0]-i);
            if (index[i] < 0){
                index[i] = 0;
            }
            end = index[i]>0 ? i : end;
        }else{
            index[i] = 0;
            if (index[0]==0){
                index[1] = order;
            }
        }
    }

    long long address;
    long long newaddress;
    if (index[0] == 1){
        if (end == 0 && inode->size % 2 == 0){
            if ((newaddress = get_block(parent->disk)==FAILURE)){
                return NULL;
            }
            inode->iblock = newaddress;
            end ++;
        }
        address = inode->iblock;
    }else if (index[0] == 2){
        if (end == 0 && inode->size % 2 == 0){
            if ((newaddress = get_block(parent->disk)==FAILURE)){
                return NULL;
            }
            inode->i2block = newaddress;
            end ++;
        }
        address = inode->i2block;
    }else if (index[0] == 3){
        if (end == 0 && inode->size % 2 == 0){
            if ((newaddress = get_block(parent->disk)==FAILURE)){
                return NULL;
            }
            inode->i3block = newaddress;
            end ++;
        }
        address = inode->i3block;
    }else if (index[0] == 4){
        if (end == 0 && inode->size % 2 == 0){
            if ((newaddress = get_block(parent->disk)==FAILURE)){
                return NULL;
            }
            inode->i4block = newaddress;
            end ++;
        }
        address = inode->i4block;
    }

    if (inode->size % 2 == 0){
        if (index[0] == 0){
            if ((newaddress = get_block(parent->disk)==FAILURE)){
                return NULL;
            }
            inode->dblocks[index[1]] = newaddress;
            fseek(fs, newaddress,SEEK_SET);
        }else{
            for (int i = 1; i<end ; i++){
                fseek(fs, address+sizeof(long long)*index[i],SEEK_SET);
                fread(&address, sizeof(long long),1,fs);
            }
            fseek(fs, address+sizeof(long long)*index[end],SEEK_SET);
            for (int i = end; i<=index[0]; i++){
                if ((newaddress = get_block(parent->disk)==FAILURE)){
                    return NULL;
                }
                fwrite(&newaddress, sizeof(long long), 1, fs);
                fseek(fs, newaddress,SEEK_SET);
            }
        }
    }else{
        if (index[0] == 0){
            fseek(fs, inode->dblocks[index[1]]+sizeof(dirent_t),SEEK_SET);
        }else{
            for (int i = 1; i<=index[0] ; i++){
                fseek(fs, address+sizeof(long long)*index[i],SEEK_SET);
                fread(&address, sizeof(long long),1,fs);
            }
            fseek(fs, address+sizeof(dirent_t),SEEK_SET);
        }
    }

    dirent_t* entry = malloc(sizeof(dirent_t));
    if ((entry->inode = get_inode(parent->disk))==FAILURE){
        return NULL;
    }

    strcpy(entry->name,filename);
    entry->type = F;

    fwrite(entry,sizeof(dirent_t),1,fs);
    fseek(fs,parent->inode,SEEK_SET);
    fwrite(inode,sizeof(inode_t),1,fs);

    free(inode);
    free(entry);

    return get_vnode(parent, filename);
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

void free_inode(int n_disk, long long address){
    FILE* disk = disks[n_disk];
    sb_t* sb = superblocks[n_disk];

    fseek(disk,address,SEEK_SET);
    inode_t* inode = malloc(sizeof(inode_t));
    fread(inode,sizeof(inode_t),1,disk);
    inode->size = 0;
    inode->next_inode = sb->free_inode;
    fwrite(inode,sizeof(inode_t),1,disk);
    sb->free_inode = address;
    fseek(disk,BOOTSIZE,SEEK_SET);
    fwrite(sb,sizeof(sb_t),1,disk);
}
