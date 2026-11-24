/* MuLIB workspace filesystem: symlink-rejecting descriptor-relative POSIX I/O.
 * Define MULIB_IMPL_WORKSPACE_FS in exactly one translation unit.
 * Compile with -D_POSIX_C_SOURCE=200809L. Linux recommended.
 * Paths must be relative, nonempty, and contain no empty, '.' or '..' segments.
 * No absolute paths, symlinks (at any depth), special files, or NUL bytes.
 * This prevents traversal escapes; it does NOT block preexisting hard links,
 * bind mounts or concurrent uncooperative writers. Use OS sandboxing as well.
 */
#ifndef MULIB_WORKSPACE_FS_H
#define MULIB_WORKSPACE_FS_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_wfs { int root_fd; } mulib_wfs_t;
#define MULIB_WFS_INIT {-1}
typedef enum mulib_wfs_status {
    MULIB_WFS_OK = 0, MULIB_WFS_INVALID = -1, MULIB_WFS_SYSTEM = -2,
    MULIB_WFS_DENIED = -3, MULIB_WFS_NOT_FOUND = -4,
    MULIB_WFS_EXISTS = -5, MULIB_WFS_LIMIT = -6,
    MULIB_WFS_NOMEM = -7, MULIB_WFS_NOT_REGULAR = -8,
    MULIB_WFS_CONFLICT = -9, MULIB_WFS_UNCERTAIN = -10
} mulib_wfs_status_t;
typedef struct mulib_wfs_blob { unsigned char *data; size_t length; } mulib_wfs_blob_t;
typedef struct mulib_wfs_stat { uint64_t size; uint32_t mode; uint64_t device, inode; } mulib_wfs_stat_t;
/* All paths are UTF-8-agnostic byte names and capped at 4096 bytes.
 * A leading '.' in a name such as '.config' is allowed. */
int mulib_wfs_validate(const char *relative);
int mulib_wfs_open(mulib_wfs_t *ws, const char *directory);
void mulib_wfs_close(mulib_wfs_t *ws);
int mulib_wfs_stat(const mulib_wfs_t *ws, const char *relative, mulib_wfs_stat_t *out);
/* Read a regular file, zero-pad one sentinel byte, limit is a hard maximum;
 * *out is unchanged on failure. Caller frees via mulib_wfs_blob_free. */
int mulib_wfs_read(const mulib_wfs_t *ws, const char *relative,
                   size_t limit, mulib_wfs_blob_t *out);
void mulib_wfs_blob_free(mulib_wfs_blob_t *blob);
/* Write/replace a regular file by same-directory temporary + rename + fsync.
 * With replace=0 target must be absent (Linux renameat2 or linkat fallback).
 * This is one-file atomic visibility, NOT a multi-file transaction or CAS.
 * Mode is used on create; existing file mode is preserved on replacement.
 * On parent fsync failure returns UNCERTAIN: rename may have committed. */
int mulib_wfs_write(const mulib_wfs_t *ws, const char *relative,
                    const void *data, size_t length, mode_t mode, int replace);
/* Remove only regular files. Directory and symlink removal is forbidden. */
int mulib_wfs_remove(const mulib_wfs_t *ws, const char *relative);
/* Internal but public for cooperating transactions: resolves a validated path
 * to an owned parent directory descriptor and malloc-owned basename. */
int mulib_wfs_parent(const mulib_wfs_t *ws, const char *relative,
                     int *parent_fd, char **basename);
/* Cryptographically unpredictable temp names; never reuses a chosen name. */
int mulib_wfs_temporary(int parent_fd, char name[48], mode_t mode);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_WORKSPACE_FS) && !defined(MULIB_WORKSPACE_FS_IMPLEMENTED)
#define MULIB_WORKSPACE_FS_IMPLEMENTED
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/fs.h>
#endif
#ifndef O_CLOEXEC
#error "mulib-workspace-fs requires O_CLOEXEC"
#endif
#ifndef O_NOFOLLOW
#error "mulib-workspace-fs requires O_NOFOLLOW"
#endif
#ifndef O_DIRECTORY
#error "mulib-workspace-fs requires O_DIRECTORY"
#endif
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_wfs_error_(int e) {
    if (e==ENOENT) return MULIB_WFS_NOT_FOUND;
    if (e==EEXIST) return MULIB_WFS_EXISTS;
    if (e==ELOOP || e==ENOTDIR || e==EACCES || e==EPERM || e==EISDIR)
        return MULIB_WFS_DENIED;
    if (e==ENOMEM) return MULIB_WFS_NOMEM;
    if (e==ENAMETOOLONG || e==EFBIG || e==EOVERFLOW) return MULIB_WFS_LIMIT;
    return MULIB_WFS_SYSTEM;
}
int mulib_wfs_validate(const char *p) {
    if (!p || !*p) return MULIB_WFS_INVALID;
    size_t n=0, segment=0;
    for (const unsigned char *s=(const unsigned char*)p; *s; ++s) {
        if (++n>4096) return MULIB_WFS_LIMIT;
        if (*s=='\\') return MULIB_WFS_DENIED; /* portable conservative policy */
        if (*s=='/') {
            if (segment==0) return MULIB_WFS_DENIED;
            segment=0;
        } else {++segment;}
    }
    if (!segment) return MULIB_WFS_DENIED;
    const char *start=p;
    for (const char *s=p;;++s) {
        if (*s=='/' || *s=='\0') {
            size_t z=(size_t)(s-start);
            if ((z==1 && start[0]=='.') || (z==2 && start[0]=='.' && start[1]=='.'))
                return MULIB_WFS_DENIED;
            if (!*s) break;
            start=s+1;
        }
    }
    return MULIB_WFS_OK;
}
int mulib_wfs_open(mulib_wfs_t *ws, const char *directory) {
    if (!ws || ws->root_fd!=-1 || !directory || !*directory) return MULIB_WFS_INVALID;
    int fd=open(directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (fd<0) return mulib_wfs_error_(errno);
    ws->root_fd=fd;return MULIB_WFS_OK;
}
void mulib_wfs_close(mulib_wfs_t *ws) {
    if (!ws) return;
    if (ws->root_fd>=0) close(ws->root_fd);
    ws->root_fd=-1;
}
int mulib_wfs_parent(const mulib_wfs_t *ws,const char *relative,int *parent_fd,char **basename) {
    if (!ws || ws->root_fd<0 || !parent_fd || !basename) return MULIB_WFS_INVALID;
    int rc=mulib_wfs_validate(relative);if (rc) return rc;
    int fd=dup(ws->root_fd);if(fd<0) return mulib_wfs_error_(errno);
    const char *component=relative, *s=relative;
    for (;;) {
        while (*s && *s!='/') ++s;
        if (!*s) break;
        size_t len=(size_t)(s-component);
        char *part=malloc(len+1);
        if(!part){close(fd);return MULIB_WFS_NOMEM;}
        memcpy(part,component,len);part[len]=0;
        int next=openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        int e=errno;free(part);close(fd);
        if(next<0) return mulib_wfs_error_(e);
        fd=next;
        component=++s;
    }
    char *name=strdup(component);
    if(!name){close(fd);return MULIB_WFS_NOMEM;}
    *parent_fd=fd;*basename=name;return MULIB_WFS_OK;
}
static int mulib_wfs_regular_(int fd,struct stat *st) {
    if(fstat(fd,st)<0) return mulib_wfs_error_(errno);
    return S_ISREG(st->st_mode)?MULIB_WFS_OK:MULIB_WFS_NOT_REGULAR;
}
int mulib_wfs_stat(const mulib_wfs_t *ws,const char *path,mulib_wfs_stat_t *out) {
    if(!out)return MULIB_WFS_INVALID;
    int parent;char *name;int rc=mulib_wfs_parent(ws,path,&parent,&name);if(rc)return rc;
    int fd=openat(parent,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    int e=errno;free(name);close(parent);
    if(fd<0)return mulib_wfs_error_(e);
    struct stat st;rc=mulib_wfs_regular_(fd,&st);close(fd);if(rc)return rc;
    if(st.st_size<0)return MULIB_WFS_LIMIT;
    mulib_wfs_stat_t info={(uint64_t)st.st_size,(uint32_t)(st.st_mode&07777),
        (uint64_t)st.st_dev,(uint64_t)st.st_ino};
    *out=info;return MULIB_WFS_OK;
}
void mulib_wfs_blob_free(mulib_wfs_blob_t *blob){if(blob){free(blob->data);blob->data=NULL;blob->length=0;}}
int mulib_wfs_read(const mulib_wfs_t *ws,const char *path,size_t limit,mulib_wfs_blob_t *out) {
    if(!out || limit==SIZE_MAX)return MULIB_WFS_INVALID;
    int parent;char*name;int rc=mulib_wfs_parent(ws,path,&parent,&name);if(rc)return rc;
    int fd=openat(parent,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    int e=errno;free(name);close(parent);
    if(fd<0)return mulib_wfs_error_(e);
    struct stat st;rc=mulib_wfs_regular_(fd,&st);if(rc){close(fd);return rc;}
    if(st.st_size<0 || (uintmax_t)st.st_size>(uintmax_t)limit){close(fd);return MULIB_WFS_LIMIT;}
    size_t cap=(size_t)st.st_size;
    if(cap>limit){close(fd);return MULIB_WFS_LIMIT;}
    if(cap<4096)cap=limit<4096?limit:4096;
    unsigned char *p=malloc(cap+1);if(!p){close(fd);return MULIB_WFS_NOMEM;}
    size_t n=0;
    for(;;) {
        if(n==cap){if(cap==limit){unsigned char extra;ssize_t got=read(fd,&extra,1);if(got<0&&errno==EINTR)continue;if(got<0){rc=mulib_wfs_error_(errno);break;}if(got>0)rc=MULIB_WFS_LIMIT;break;}
            size_t nc=cap?cap*2:1;if(nc<cap||nc>limit)nc=limit;
            unsigned char *np=realloc(p,nc+1);if(!np){rc=MULIB_WFS_NOMEM;break;}p=np;cap=nc;}
        ssize_t got=read(fd,p+n,cap-n);
        if(got<0){if(errno==EINTR)continue;rc=mulib_wfs_error_(errno);break;}
        if(got==0)break;
        n+=(size_t)got;
    }
    if(close(fd)<0 && !rc)rc=mulib_wfs_error_(errno);
    if(rc){free(p);return rc;}
    p[n]=0;mulib_wfs_blob_t tmp={p,n};*out=tmp;return MULIB_WFS_OK;
}
static int mulib_wfs_random_(unsigned char bytes[16]) {
#if defined(__linux__) && defined(_GNU_SOURCE)
#ifdef SYS_getrandom
    size_t n=0;while(n<16){ssize_t r=syscall(SYS_getrandom,bytes+n,16-n,0);if(r<0&&errno==EINTR)continue;if(r>0){n+=(size_t)r;continue;}break;}
    if(n==16)return 1;
#endif
#endif
    int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);if(fd<0)return 0;
    size_t got=0;while(got<16){ssize_t k=read(fd,bytes+got,16-got);if(k<0&&errno==EINTR)continue;if(k<=0)break;got+=(size_t)k;}
    close(fd);return got==16;
}
int mulib_wfs_temporary(int parent_fd,char name[48],mode_t mode) {
    if(parent_fd<0||!name||mode & ~0777) return -1;
    static const char hex[]="0123456789abcdef";
    for(int attempt=0;attempt<16;attempt++) {
        unsigned char bytes[16];if(!mulib_wfs_random_(bytes))return -1;
        memcpy(name,".mulib-wfs-",11);
        for(size_t i=0;i<16;i++){name[11+i*2]=hex[bytes[i]>>4];name[12+i*2]=hex[bytes[i]&15];}
        name[43]=0;
        int fd=openat(parent_fd,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,mode);
        if(fd>=0)return fd;
        if(errno!=EEXIST)return -1;
    }
    errno=EEXIST;return -1;
}
static int mulib_wfs_write_all_(int fd,const unsigned char *p,size_t n) {
    while(n){size_t m=n>(size_t)SSIZE_MAX?(size_t)SSIZE_MAX:n;ssize_t k=write(fd,p,m);
        if(k<0&&errno==EINTR)continue;
        if(k<=0)return 0;
        p+=(size_t)k;n-=(size_t)k;}
    return 1;
}
int mulib_wfs_write(const mulib_wfs_t *ws,const char *path,const void *data,size_t length,mode_t mode,int replace) {
    if((!data&&length) || (mode&~0777) || (replace!=0&&replace!=1))return MULIB_WFS_INVALID;
    int parent;char *name;int rc=mulib_wfs_parent(ws,path,&parent,&name);if(rc)return rc;
    struct stat old;int existed=0;
    if(fstatat(parent,name,&old,AT_SYMLINK_NOFOLLOW)==0){existed=1;
        if(!S_ISREG(old.st_mode)){rc=MULIB_WFS_NOT_REGULAR;goto done;}
        if(!replace){rc=MULIB_WFS_EXISTS;goto done;}
        mode=old.st_mode&0777;
    }else if(errno!=ENOENT){rc=mulib_wfs_error_(errno);goto done;}
    char temp[48];int fd=mulib_wfs_temporary(parent,temp,mode&0777);
    if(fd<0){rc=mulib_wfs_error_(errno);goto done;}
    if(fchmod(fd,mode&0777)<0 || !mulib_wfs_write_all_(fd,data,length)||fsync(fd)<0){
        rc=MULIB_WFS_SYSTEM;close(fd);unlinkat(parent,temp,0);goto done;}
    if(close(fd)<0){rc=mulib_wfs_error_(errno);unlinkat(parent,temp,0);goto done;}
    /* Prevent accidental replacement of pre-existing symlinks and special nodes. */
    struct stat current;
    if(fstatat(parent,name,&current,AT_SYMLINK_NOFOLLOW)==0){
        if(!S_ISREG(current.st_mode)){rc=MULIB_WFS_DENIED;unlinkat(parent,temp,0);goto done;}
        if(!replace || !existed || current.st_dev!=old.st_dev || current.st_ino!=old.st_ino){
            rc=MULIB_WFS_CONFLICT;unlinkat(parent,temp,0);goto done;}
    }else if(errno!=ENOENT){rc=mulib_wfs_error_(errno);unlinkat(parent,temp,0);goto done;}
    else if(existed){rc=MULIB_WFS_CONFLICT;unlinkat(parent,temp,0);goto done;}
    if(!replace) {
        if(linkat(parent,temp,parent,name,0)<0){rc=mulib_wfs_error_(errno);unlinkat(parent,temp,0);goto done;}
        if(unlinkat(parent,temp,0)<0){rc=MULIB_WFS_UNCERTAIN;goto done;}
    } else if(renameat(parent,temp,parent,name)<0){rc=mulib_wfs_error_(errno);unlinkat(parent,temp,0);goto done;}
    if(fsync(parent)<0){rc=MULIB_WFS_UNCERTAIN;goto done;}
    rc=MULIB_WFS_OK;
done:free(name);close(parent);return rc;
}
int mulib_wfs_remove(const mulib_wfs_t *ws,const char *path){
    int parent;char *name;int rc=mulib_wfs_parent(ws,path,&parent,&name);if(rc)return rc;
    struct stat st;
    if(fstatat(parent,name,&st,AT_SYMLINK_NOFOLLOW)<0)rc=mulib_wfs_error_(errno);
    else if(!S_ISREG(st.st_mode))rc=MULIB_WFS_NOT_REGULAR;
    else if(unlinkat(parent,name,0)<0)rc=mulib_wfs_error_(errno);
    else if(fsync(parent)<0)rc=MULIB_WFS_UNCERTAIN;
    else rc=MULIB_WFS_OK;
    free(name);close(parent);return rc;
}
#ifdef __cplusplus
}
#endif
#endif
