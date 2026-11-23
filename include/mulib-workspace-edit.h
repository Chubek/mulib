/* MuLIB cooperating workspace edit batch, C11/POSIX single-header.
 * Define MULIB_IMPL_WORKSPACE_EDIT and implementation switches for WFS/UNIDIFF.
 * Exact preimage checks, staged replacements, file modes, best-effort rollback,
 * and serialized participating writers via a root lock. Not crash-atomic across
 * multiple paths; externally uncooperative writers are outside this guarantee.
 * The caller must authorize every edit; this is NOT a permission mechanism.
 */
#ifndef MULIB_WORKSPACE_EDIT_H
#define MULIB_WORKSPACE_EDIT_H
#include "mulib-workspace-fs.h"
#include "mulib-unidiff.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_we_kind {MULIB_WE_PUT=1,MULIB_WE_DELETE=2} mulib_we_kind_t;
typedef struct mulib_we_edit {
    const char *path;mulib_we_kind_t kind;
    int expected_present; /* 1: exact bytes must exist, 0: must be absent */
    const void *expected;size_t expected_length;
    const void *replacement;size_t replacement_length;
    mode_t create_mode; /* creation only; default 0600 if zero */
} mulib_we_edit_t;
typedef struct mulib_we_limits {size_t max_files,max_file_bytes,max_total_bytes;} mulib_we_limits_t;
typedef enum mulib_we_phase {MULIB_WE_STAGED=1,MULIB_WE_COMMITTED=2,
    MULIB_WE_ROLLED_BACK=3,MULIB_WE_FAILED=4} mulib_we_phase_t;
typedef struct mulib_we_event {size_t index;const char *path;
    mulib_we_phase_t phase;size_t before_bytes,after_bytes;int status;
    const char *backup_basename; /* only for failed rollback; borrowed */
} mulib_we_event_t;
/* Audit notifications are synchronous and informational. Host must queue them
 * without calling back into this kernel and persist them if required. */
typedef void (*mulib_we_audit_fn)(void *user,const mulib_we_event_t *event);
typedef struct mulib_we_report {size_t staged,applied,rolled_back,failed_index;
    int rollback_errors,commit_started;} mulib_we_report_t;
/* Codes 0 success, negatives are WFS codes; CONFLICT for wrong preimages.
 * If rollback fails, UNCERTAIN is returned. Caller owns a clean initial report.
 * On success all files have been renamed and parent directories fsynced.
 * If an fsync fails, UNCERTAIN may mean a file was already changed. */
int mulib_we_apply_audited(const mulib_wfs_t *workspace,const mulib_we_edit_t *edits,
    size_t count,const mulib_we_limits_t *limits,mulib_we_report_t *report,
    mulib_we_audit_fn on_event,void *event_user);
int mulib_we_apply(const mulib_wfs_t *workspace,const mulib_we_edit_t *edits,
    size_t count,const mulib_we_limits_t *limits,mulib_we_report_t *report);
/* Apply parsed unified diff to actual files, using exact preimage snapshots.
 * /dev/null means create/delete. No file renames or binary patches. */
int mulib_we_apply_unidiff(const mulib_wfs_t *workspace,
    const mulib_ud_patch_t *patch,const mulib_we_limits_t *limits,
    mulib_we_report_t *report);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_WORKSPACE_EDIT) && !defined(MULIB_WORKSPACE_EDIT_IMPLEMENTED)
#define MULIB_WORKSPACE_EDIT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_we_stage_ {int parent;char *name;char stage[48],backup[48];
    int staged,backed,committed,had_old;mode_t mode;} mulib_we_stage_;
static mulib_we_limits_t mulib_we_limits_(const mulib_we_limits_t *p){
    if(p)return *p;
    mulib_we_limits_t l={128,16u*1024*1024,64u*1024*1024};return l;
}
static int mulib_we_add_(size_t *n,size_t amount,size_t max){
    if(amount>max-*n)return 0;
    *n+=amount;return 1;
}
static int mulib_we_write_(int fd,const void *p,size_t n){
    const unsigned char *b=p;
    while(n){size_t len=n>(size_t)SSIZE_MAX?(size_t)SSIZE_MAX:n;
        ssize_t z=write(fd,b,len);
        if(z<0 && errno==EINTR)continue;
        if(z<=0)return 0;
        n-=(size_t)z;b+=(size_t)z;
    }return 1;
}
/* Obtain unique backup name by creating a cryptographically named file then
 * unlinking it before linkat. linkat is exclusive, so collisions fail closed. */
static int mulib_we_backup_(mulib_we_stage_ *s) {
    int fd=mulib_wfs_temporary(s->parent,s->backup,0600);
    if(fd<0)return MULIB_WFS_SYSTEM;
    int close_ok=close(fd)==0;
    int unlink_ok=unlinkat(s->parent,s->backup,0)==0;
    if(!close_ok || !unlink_ok)return MULIB_WFS_SYSTEM;
    if(linkat(s->parent,s->name,s->parent,s->backup,0)<0)return MULIB_WFS_SYSTEM;
    s->backed=1;return 0;
}
static void mulib_we_cleanup_(mulib_we_stage_ *s,size_t n,int retain_backups){
    for(size_t i=0;i<n;i++){
        if(s[i].parent>=0){
            if(s[i].staged)unlinkat(s[i].parent,s[i].stage,0);
            if(s[i].backed && !retain_backups)unlinkat(s[i].parent,s[i].backup,0);
            close(s[i].parent);
        }
        free(s[i].name);
    }
    free(s);
}
static void mulib_we_emit_(mulib_we_audit_fn audit,void *user,
    const mulib_we_edit_t *edit,size_t index,mulib_we_phase_t phase,int status){
    if(!audit)return;
    mulib_we_event_t event={index,edit[index].path,phase,
        edit[index].expected_length,edit[index].replacement_length,status,NULL};
    audit(user,&event);
}
int mulib_we_apply_audited(const mulib_wfs_t *ws,const mulib_we_edit_t *e,
    size_t n,const mulib_we_limits_t *limits,mulib_we_report_t *out,
    mulib_we_audit_fn audit,void *audit_user){
    mulib_we_report_t report={0};report.failed_index=SIZE_MAX;
    mulib_we_limits_t lim=mulib_we_limits_(limits);
    if(out)*out=report;
    if(!ws||ws->root_fd<0||(!e&&n)||!n||n>lim.max_files||!lim.max_file_bytes||!lim.max_total_bytes)
        return MULIB_WFS_INVALID;
    size_t total=0;
    for(size_t i=0;i<n;i++){
        if(!e[i].path || (e[i].expected_present!=0&&e[i].expected_present!=1) ||
           (e[i].kind!=MULIB_WE_PUT&&e[i].kind!=MULIB_WE_DELETE)||
           (!e[i].expected && e[i].expected_length)||
           (!e[i].replacement && e[i].replacement_length)||
           (e[i].kind==MULIB_WE_DELETE&&(!e[i].expected_present||e[i].replacement_length))||
           e[i].create_mode&~0777 || e[i].expected_length>lim.max_file_bytes||
           e[i].replacement_length>lim.max_file_bytes ||
           !mulib_we_add_(&total,e[i].expected_length,lim.max_total_bytes)||
           !mulib_we_add_(&total,e[i].replacement_length,lim.max_total_bytes))return MULIB_WFS_INVALID;
        int v=mulib_wfs_validate(e[i].path);if(v)return v;
        if(!strcmp(e[i].path,".mulib-we.lock")||
           !strncmp(e[i].path,".mulib-wfs-",11))return MULIB_WFS_DENIED;
        for(size_t j=0;j<i;j++)if(!strcmp(e[j].path,e[i].path))return MULIB_WFS_CONFLICT;
    }
    /* This lock coordinates multiple harnesses honoring the same protocol. */
    int lockfd=openat(ws->root_fd,".mulib-we.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
    if(lockfd<0)return MULIB_WFS_DENIED;
    if(flock(lockfd,LOCK_EX)<0){close(lockfd);return MULIB_WFS_SYSTEM;}
    mulib_we_stage_ *st=calloc(n,sizeof *st);
    if(!st){close(lockfd);return MULIB_WFS_NOMEM;}
    for(size_t i=0;i<n;i++)st[i].parent=-1;
    int rc=MULIB_WFS_OK;
    for(size_t i=0;i<n;i++){
        report.failed_index=i;
        rc=mulib_wfs_parent(ws,e[i].path,&st[i].parent,&st[i].name);
        if(rc)goto fail;
        mulib_wfs_blob_t old={0};
        rc=mulib_wfs_read(ws,e[i].path,lim.max_file_bytes,&old);
        if(rc==MULIB_WFS_NOT_FOUND){if(e[i].expected_present){rc=MULIB_WFS_CONFLICT;goto fail;}
            rc=MULIB_WFS_OK;st[i].had_old=0;}
        else if(rc){goto fail;}
        else {
            st[i].had_old=1;
            if(!e[i].expected_present||old.length!=e[i].expected_length||
               (old.length&&memcmp(old.data,e[i].expected,old.length))){
                mulib_wfs_blob_free(&old);rc=MULIB_WFS_CONFLICT;goto fail;}
            mulib_wfs_stat_t info;
            rc=mulib_wfs_stat(ws,e[i].path,&info);
            if(rc){mulib_wfs_blob_free(&old);goto fail;}
            st[i].mode=(mode_t)(info.mode&0777);
        }
        mulib_wfs_blob_free(&old);
        if(st[i].had_old){rc=mulib_we_backup_(&st[i]);if(rc)goto fail;}
        if(e[i].kind==MULIB_WE_PUT){
            mode_t mode=st[i].had_old?st[i].mode:(e[i].create_mode?e[i].create_mode:0600);
            int fd=mulib_wfs_temporary(st[i].parent,st[i].stage,mode&0777);
            if(fd<0){rc=MULIB_WFS_SYSTEM;goto fail;}
            st[i].staged=1;
            if(fchmod(fd,mode&0777)<0||
               !mulib_we_write_(fd,e[i].replacement,e[i].replacement_length)||
               fsync(fd)<0){rc=MULIB_WFS_SYSTEM;close(fd);goto fail;}
            if(close(fd)<0){rc=MULIB_WFS_SYSTEM;goto fail;}
        }
        report.staged++;
        mulib_we_emit_(audit,audit_user,e,i,MULIB_WE_STAGED,0);
    }
    /* Recheck identity and bytes immediately before commitment. For hardlink
     * backups, compare snapshot via link inode; in-place uncooperative writes
     * can still race this check. */
    for(size_t i=0;i<n;i++){
        mulib_wfs_blob_t now={0};
        rc=mulib_wfs_read(ws,e[i].path,lim.max_file_bytes,&now);
        if(!st[i].had_old){if(rc==MULIB_WFS_NOT_FOUND)rc=0;else if(rc==0){mulib_wfs_blob_free(&now);rc=MULIB_WFS_CONFLICT;}if(rc)goto fail;}
        else {
            if(rc)goto fail;
            if(now.length!=e[i].expected_length||
               (now.length&&memcmp(now.data,e[i].expected,now.length))){
                mulib_wfs_blob_free(&now);rc=MULIB_WFS_CONFLICT;goto fail;}
            mulib_wfs_blob_free(&now);
            struct stat curr,bak;
            if(fstatat(st[i].parent,st[i].name,&curr,AT_SYMLINK_NOFOLLOW)<0||
               fstatat(st[i].parent,st[i].backup,&bak,AT_SYMLINK_NOFOLLOW)<0||
               curr.st_dev!=bak.st_dev||curr.st_ino!=bak.st_ino){rc=MULIB_WFS_CONFLICT;goto fail;}
        }
    }
    report.commit_started=1;
    for(size_t i=0;i<n;i++){
        report.failed_index=i;
        if(e[i].kind==MULIB_WE_DELETE){
            if(unlinkat(st[i].parent,st[i].name,0)<0){rc=MULIB_WFS_SYSTEM;goto rollback;}
        }else {
            if(renameat(st[i].parent,st[i].stage,st[i].parent,st[i].name)<0){rc=MULIB_WFS_SYSTEM;goto rollback;}
            st[i].staged=0;
        }
        st[i].committed=1;report.applied++;
        mulib_we_emit_(audit,audit_user,e,i,MULIB_WE_COMMITTED,0);
        if(fsync(st[i].parent)<0){rc=MULIB_WFS_UNCERTAIN;goto rollback;}
    }
    for(size_t i=0;i<n;i++)if(st[i].backed){
        if(unlinkat(st[i].parent,st[i].backup,0)<0){rc=MULIB_WFS_UNCERTAIN;goto fail;}
        st[i].backed=0;
        if(fsync(st[i].parent)<0){rc=MULIB_WFS_UNCERTAIN;goto fail;}
    }
    report.failed_index=SIZE_MAX;
    rc=0;goto fail;
rollback:
    for(size_t j=report.applied;j>0;j--){
        size_t i=j-1;int ok=1;
        if(st[i].had_old){
            if(renameat(st[i].parent,st[i].backup,st[i].parent,st[i].name)<0)ok=0;
            else st[i].backed=0;
        }else if(unlinkat(st[i].parent,st[i].name,0)<0)ok=0;
        if(ok&&fsync(st[i].parent)<0)ok=0;
        if(ok)report.rolled_back++;else report.rollback_errors++;
        if(!ok && audit && st[i].backed){
            mulib_we_event_t incident={i,e[i].path,MULIB_WE_FAILED,
                e[i].expected_length,e[i].replacement_length,
                MULIB_WFS_UNCERTAIN,st[i].backup};
            audit(audit_user,&incident);
        }
        mulib_we_emit_(audit,audit_user,e,i,ok?MULIB_WE_ROLLED_BACK:MULIB_WE_FAILED,
                       ok?0:MULIB_WFS_UNCERTAIN);
    }
    if(report.rollback_errors)rc=MULIB_WFS_UNCERTAIN;
fail:
    if(rc && report.failed_index<n)
        mulib_we_emit_(audit,audit_user,e,report.failed_index,MULIB_WE_FAILED,rc);
    mulib_we_cleanup_(st,n,report.rollback_errors!=0);
    if(flock(lockfd,LOCK_UN)<0 && !rc)rc=MULIB_WFS_UNCERTAIN;
    close(lockfd);
    if(out)*out=report;
    return rc;
}
int mulib_we_apply(const mulib_wfs_t *ws,const mulib_we_edit_t *e,
    size_t n,const mulib_we_limits_t *lim,mulib_we_report_t *report){
    return mulib_we_apply_audited(ws,e,n,lim,report,NULL,NULL);
}
int mulib_we_apply_unidiff(const mulib_wfs_t *ws,const mulib_ud_patch_t *patch,
    const mulib_we_limits_t *limits,mulib_we_report_t *report){
    mulib_we_limits_t lim=mulib_we_limits_(limits);
    if(report){memset(report,0,sizeof *report);report->failed_index=SIZE_MAX;}
    if(!ws||!patch||!patch->file_count||patch->file_count>lim.max_files)return MULIB_WFS_INVALID;
    size_t n=patch->file_count;
    mulib_we_edit_t *edits=calloc(n,sizeof *edits);
    mulib_wfs_blob_t *old=calloc(n,sizeof *old);
    mulib_ud_blob_t *fresh=calloc(n,sizeof *fresh);
    if(!edits||!old||!fresh){free(edits);free(old);free(fresh);return MULIB_WFS_NOMEM;}
    int rc=0;
    for(size_t i=0;i<n;i++){
        const mulib_ud_file_t *f=&patch->files[i];
        int existed=*f->old_path!=0;
        const char *path=existed?f->old_path:f->new_path;
        if(existed){rc=mulib_wfs_read(ws,path,lim.max_file_bytes,&old[i]);if(rc)goto done;}
        rc=mulib_ud_apply(f,old[i].data,old[i].length,lim.max_file_bytes,
                          &fresh[i]);
        if(rc){rc=rc==MULIB_UD_CONFLICT?MULIB_WFS_CONFLICT:MULIB_WFS_INVALID;goto done;}
        edits[i].path=path;
        edits[i].kind=*f->new_path?MULIB_WE_PUT:MULIB_WE_DELETE;
        edits[i].expected_present=existed;
        edits[i].expected=old[i].data;edits[i].expected_length=old[i].length;
        edits[i].replacement=fresh[i].data;edits[i].replacement_length=fresh[i].length;
        edits[i].create_mode=0644;
    }
    rc=mulib_we_apply(ws,edits,n,limits,report);
done:
    for(size_t i=0;i<n;i++){mulib_wfs_blob_free(&old[i]);mulib_ud_blob_free(&fresh[i]);}
    free(edits);free(old);free(fresh);return rc;
}
#ifdef __cplusplus
}
#endif
#endif
