#include "mulib-workspace-edit.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
static int fail_next_commit;
static int fail_directory_sync;
int __real_renameat(int from, const char *source, int to, const char *target);
int __wrap_renameat(int from, const char *source, int to, const char *target){
    if(fail_next_commit && !strcmp(target,"b")){
        fail_next_commit=0;
        errno=EIO;
        return -1;
    }
    return __real_renameat(from,source,to,target);
}
int __real_fsync(int fd);
int __wrap_fsync(int fd){
    if(fail_directory_sync==2){
        fail_directory_sync=0;
        errno=EIO;
        return -1;
    }
    return __real_fsync(fd);
}
static int rollback_events=0;
static void audit(void *u,const mulib_we_event_t *e){
    (void)u;
    if(e->phase==MULIB_WE_ROLLED_BACK)rollback_events++;
    if(e->phase==MULIB_WE_COMMITTED && fail_directory_sync==1)fail_directory_sync=2;
}
static void check_restored(const mulib_wfs_t *ws, ino_t inode){
    struct stat restored;
    assert(fstatat(ws->root_fd,"a",&restored,0)==0 && restored.st_ino==inode);
    mulib_wfs_blob_t b={0};assert(mulib_wfs_read(ws,"a",100,&b)==0);
    assert(b.length==3&&!memcmp(b.data,"old",3));mulib_wfs_blob_free(&b);
    assert(mulib_wfs_read(ws,"b",100,&b)==MULIB_WFS_NOT_FOUND);
}
int main(void){
    char dir[]="/tmp/mulib-rollback-XXXXXX";assert(mkdtemp(dir));
    mulib_wfs_t ws=MULIB_WFS_INIT;assert(mulib_wfs_open(&ws,dir)==0);
    assert(mulib_wfs_write(&ws,"a","old",3,0600,0)==0);
    struct stat original;
    assert(fstatat(ws.root_fd,"a",&original,0)==0);
    mulib_we_edit_t changes[]={
        {"a",MULIB_WE_PUT,1,"old",3,"NEW",3,0},
        {"b",MULIB_WE_PUT,0,NULL,0,"X",1,0600}};
    mulib_we_report_t report={0};
    fail_next_commit=1;
    int rc=mulib_we_apply_audited(&ws,changes,2,NULL,&report,audit,NULL);
    assert(rc==MULIB_WFS_SYSTEM && !fail_next_commit && report.applied==1 &&
           report.rolled_back==1 && report.rollback_errors==0 && rollback_events==1);
    check_restored(&ws,original.st_ino);
    rollback_events=0;
    fail_directory_sync=1;
    rc=mulib_we_apply_audited(&ws,changes,2,NULL,&report,audit,NULL);
    assert(rc==MULIB_WFS_UNCERTAIN && !fail_directory_sync && report.applied==1 &&
           report.rolled_back==1 && report.rollback_errors==0 && rollback_events==1);
    check_restored(&ws,original.st_ino);
    assert(unlinkat(ws.root_fd,"a",0)==0);
    assert(unlinkat(ws.root_fd,".mulib-we.lock",0)==0);
    mulib_wfs_close(&ws);
    assert(rmdir(dir)==0);
    puts("PASS: rename and directory fsync failures restored the original inode");
    return 0;
}
