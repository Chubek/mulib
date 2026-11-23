#include "mulib-workspace-fs.h"
#include "mulib-unidiff.h"
#include "mulib-workspace-edit.h"
#include "mulib-process-stream.h"
#include "mulib-fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#define CHECK(condition) do {if(!(condition)){fprintf(stderr,"CHECK FAILED %s:%d: %s\n", __FILE__,__LINE__,#condition);abort();}}while(0)
static char root_path[128];
static mulib_wfs_t ws=MULIB_WFS_INIT;
static void test_file(const char *path,const void *data,size_t n){
  mulib_wfs_blob_t b={0};CHECK(mulib_wfs_read(&ws,path,1024*1024,&b)==0);
  CHECK(b.length==n);CHECK(!memcmp(b.data,data,n));mulib_wfs_blob_free(&b);
}
static void test_wfs(void){
    CHECK(mulib_wfs_validate("hello")==0);
    CHECK(mulib_wfs_validate(".hidden/sub.a")==0);
    const char *bad[]={"","/etc/passwd","../etc/passwd","file/../x","foo//bar","./a","a/.","a/","a\\b",NULL};
    for(size_t i=0;bad[i];i++)CHECK(mulib_wfs_validate(bad[i])<0);
    CHECK(mkdirat(ws.root_fd,"sub",0700)==0);
    const unsigned char binary[]={0,'x',0,'y'};
    CHECK(mulib_wfs_write(&ws,"sub/data",binary,sizeof binary,0640,0)==0);
    test_file("sub/data",binary,sizeof binary);
    mulib_wfs_stat_t st={0};CHECK(mulib_wfs_stat(&ws,"sub/data",&st)==0);
    CHECK(st.mode==0640&&st.size==4);
    CHECK(mulib_wfs_write(&ws,"sub/data","REPL",4,0600,1)==0);
    test_file("sub/data","REPL",4);
    CHECK(mulib_wfs_stat(&ws,"sub/data",&st)==0&&st.mode==0640);
    CHECK(mulib_wfs_write(&ws,"sub/data","X",1,0600,0)==MULIB_WFS_EXISTS);
    CHECK(symlinkat("/etc/passwd",ws.root_fd,"bad")==0);
    mulib_wfs_blob_t blob={0};
    CHECK(mulib_wfs_read(&ws,"bad",2000,&blob)<0);
    CHECK(mulib_wfs_write(&ws,"bad","X",1,0600,1)<0);
    CHECK(mulib_wfs_remove(&ws,"bad")<0);
    CHECK(symlinkat("/etc",ws.root_fd,"linkdir")==0);
    CHECK(mulib_wfs_read(&ws,"linkdir/passwd",1024,&blob)<0);
    CHECK(mulib_wfs_write(&ws,"linkdir/file","X",1,0600,1)<0);
    CHECK(mulib_wfs_read(&ws,"sub/data",1,&blob)==MULIB_WFS_LIMIT);
    CHECK(mulib_wfs_read(&ws,"sub/data",4,&blob)==0&&blob.length==4);
    mulib_wfs_blob_free(&blob);
    CHECK(mulib_wfs_remove(&ws,"sub/data")==0);
    CHECK(mulib_wfs_read(&ws,"sub/data",100,&blob)==MULIB_WFS_NOT_FOUND);
    CHECK(mulib_wfs_write(&ws,"empty",NULL,0,0600,0)==0);
    CHECK(mulib_wfs_read(&ws,"empty",0,&blob)==0&&blob.length==0);
    mulib_wfs_blob_free(&blob);
}
static void parse(const char *s,mulib_ud_patch_t *p){
    mulib_ud_error_t e={0};int rc=mulib_ud_parse(s,strlen(s),NULL,p,&e);
    if(rc)fprintf(stderr,"parse error %d line %zu file %zu hunk %zu\n",rc,e.line,e.file,e.hunk);
    CHECK(rc==0);
}
static void test_unidiff(void){
    mulib_ud_patch_t p={0};mulib_ud_blob_t b={0};
    const char *patch="--- a/f\n+++ b/f\n@@ -1,3 +1,3 @@\n one\n-two\n+TWO\n three\n";
    parse(patch,&p);CHECK(p.file_count==1);
    const char *src="one\ntwo\nthree\n";
    CHECK(mulib_ud_apply(&p.files[0],src,strlen(src),1024,&b)==0);
    CHECK(b.length==strlen("one\nTWO\nthree\n"));
    CHECK(!memcmp(b.data,"one\nTWO\nthree\n",b.length));
    mulib_ud_blob_free(&b);
    CHECK(mulib_ud_apply(&p.files[0],"bad\n",4,1024,&b)==MULIB_UD_CONFLICT);
    mulib_ud_free(&p);
    parse("--- /dev/null\n+++ b/new\n@@ -0,0 +1,2 @@\n+hello\n+world\n",&p);
    CHECK(mulib_ud_apply(&p.files[0],NULL,0,100,&b)==0);
    CHECK(b.length==12&&!memcmp(b.data,"hello\nworld\n",12));
    mulib_ud_blob_free(&b);mulib_ud_free(&p);
    parse("--- a/old\n+++ /dev/null\n@@ -1,2 +0,0 @@\n-hello\n-world\n",&p);
    CHECK(mulib_ud_apply(&p.files[0],"hello\nworld\n",12,100,&b)==0&&b.length==0);
    mulib_ud_blob_free(&b);mulib_ud_free(&p);
    parse("--- a/foo\n+++ b/foo\n@@ -1 +1 @@\n-old\n\\ No newline at end of file\n+new\n\\ No newline at end of file\n",&p);
    CHECK(mulib_ud_apply(&p.files[0],"old",3,100,&b)==0&&b.length==3&&!memcmp(b.data,"new",3));
    mulib_ud_blob_free(&b);mulib_ud_free(&p);
    parse("--- a/foo\n+++ b/foo\n@@ -0,0 +1 @@\n+first\n@@ -1,0 +3 @@\n+last\n",&p);
    CHECK(mulib_ud_apply(&p.files[0],"middle\n",7,100,&b)==0);
    CHECK(b.length==18&&!memcmp(b.data,"first\nmiddle\nlast\n",18));
    mulib_ud_blob_free(&b);mulib_ud_free(&p);
    parse("diff --git a/f b/f\r\nindex 1234..5678 100644\r\n"
          "--- a/f\r\n+++ b/f\r\n@@ -1 +1 @@\r\n-old\r\n+new\r\n",&p);
    CHECK(mulib_ud_apply(&p.files[0],"old\n",4,100,&b)==MULIB_UD_CONFLICT);
    CHECK(mulib_ud_apply(&p.files[0],"old\r\n",5,100,&b)==0);
    CHECK(b.length==5&&!memcmp(b.data,"new\r\n",5));
    mulib_ud_blob_free(&b);mulib_ud_free(&p);
    const char *mode="old mode 100644\nnew mode 100755\n--- a/f\n+++ b/f\n@@ -1 +1 @@\n-a\n+b\n";
    CHECK(mulib_ud_parse(mode,strlen(mode),NULL,&p,NULL)==MULIB_UD_UNSUPPORTED);
    const char *invalid[]={
      "--- a/../p\n+++ b/../p\n@@ -0,0 +1 @@\n+x\n",
      "--- a/x\n+++ b/y\n@@ -1 +1 @@\n-a\n+b\n",
      "GIT binary patch\n",
      "--- a/f\n+++ b/f\n@@ -1,2 +1,1 @@\n-a\n",
      "--- a/f\n+++ b/f\n@@ -1 +1 @@\n-a\n+b\n+extra\n",
      "--- a/f\textra\n+++ b/f\textra\n@@ -1 +1 @@\n-a\n+b\n",
      "--- \"a/f\"\n+++ \"b/f\"\n@@ -1 +1 @@\n-a\n+b\n",
      "diff --cc f\n--- a/f\n+++ b/f\n@@@ -1 -1 +1 @@@\n--a\n++b\n",
      NULL};
    for(int i=0;invalid[i];i++)CHECK(mulib_ud_parse(invalid[i],strlen(invalid[i]),NULL,&p,NULL)<0);
}
typedef struct audit_counter {int stages,commits,rollbacks,failures;} audit_counter;
static void audit_cb(void *user,const mulib_we_event_t *event){
    audit_counter *a=user;
    if(event->phase==MULIB_WE_STAGED)a->stages++;
    else if(event->phase==MULIB_WE_COMMITTED)a->commits++;
    else if(event->phase==MULIB_WE_ROLLED_BACK)a->rollbacks++;
    else if(event->phase==MULIB_WE_FAILED)a->failures++;
}
static void test_edits(void){
    CHECK(mulib_wfs_write(&ws,"a","one\n",4,0640,0)==0);
    CHECK(mulib_wfs_write(&ws,"b","two\n",4,0600,0)==0);
    mulib_we_edit_t es[]={
        {"a",MULIB_WE_PUT,1,"one\n",4,"ONE\n",4,0600},
        {"b",MULIB_WE_DELETE,1,"two\n",4,NULL,0,0},
        {"c",MULIB_WE_PUT,0,NULL,0,"NEW\n",4,0644}
    };
    mulib_we_report_t r;
    audit_counter audit={0};
    CHECK(mulib_we_apply_audited(&ws,es,3,NULL,&r,audit_cb,&audit)==0);
    CHECK(audit.stages==3&&audit.commits==3&&!audit.rollbacks&&!audit.failures);
    CHECK(r.applied==3&&r.rollback_errors==0);
    test_file("a","ONE\n",4);test_file("c","NEW\n",4);
    mulib_wfs_blob_t b={0};CHECK(mulib_wfs_read(&ws,"b",100,&b)==MULIB_WFS_NOT_FOUND);
    mulib_wfs_stat_t st;CHECK(mulib_wfs_stat(&ws,"a",&st)==0&&st.mode==0640);
    CHECK(mulib_wfs_stat(&ws,"c",&st)==0&&st.mode==0644);
    CHECK(mulib_we_apply(&ws,es,3,NULL,&r)==MULIB_WFS_CONFLICT);
    test_file("a","ONE\n",4);test_file("c","NEW\n",4);
    mulib_we_edit_t dup[]={ {"a",MULIB_WE_PUT,1,"ONE\n",4,"x",1,0},
                             {"a",MULIB_WE_PUT,1,"ONE\n",4,"y",1,0}};
    CHECK(mulib_we_apply(&ws,dup,2,NULL,&r)==MULIB_WFS_CONFLICT);
    mulib_we_edit_t locked={".mulib-we.lock",MULIB_WE_DELETE,1,"",0,NULL,0,0};
    CHECK(mulib_we_apply(&ws,&locked,1,NULL,&r)==MULIB_WFS_DENIED);
    const char *patch="--- a/a\n+++ b/a\n@@ -1 +1 @@\n-ONE\n+ABC\n"
                      "--- /dev/null\n+++ b/d\n@@ -0,0 +1 @@\n+created\n";
    mulib_ud_patch_t u={0};parse(patch,&u);
    CHECK(mulib_we_apply_unidiff(&ws,&u,NULL,&r)==0);
    test_file("a","ABC\n",4);test_file("d","created\n",8);
    mulib_ud_free(&u);
    const char *bad="--- a/a\n+++ b/a\n@@ -1 +1 @@\n-MISSING\n+X\n";
    parse(bad,&u);
    CHECK(mulib_we_apply_unidiff(&ws,&u,NULL,&r)==MULIB_WFS_CONFLICT);
    test_file("a","ABC\n",4);
    mulib_ud_free(&u);
    CHECK(mulib_wfs_write(&ws,"readonly","x",1,0444,0)==0);
    test_file("readonly","x",1);
}
typedef struct output {unsigned char bytes[100000];size_t n;int stdout_seen,stderr_seen;} output;
static int collect(void *u,int stream,const unsigned char *data,size_t n){
    output *o=u;if(o->n+n>sizeof o->bytes)return 0;
    memcpy(o->bytes+o->n,data,n);o->n+=n;
    if(stream==1)o->stdout_seen=1;
    if(stream==2)o->stderr_seen=1;
    return 1;
}
static void test_process(void){
    mulib_ps_t ps=MULIB_PS_INIT;output o={0};
    const char *const args[]={"/bin/sh","-c","printf ABC; printf DEF >&2",NULL};
    mulib_ps_spec_t spec={"/bin/sh",args,NULL,NULL,0,0,0,0,5000,100000};
    CHECK(mulib_ps_start(&ps,&spec)==0);
    CHECK(mulib_ps_wait(&ps,collect,&o)==MULIB_PS_DONE);
    CHECK(ps.exit_code==0&&ps.term_signal==0);
    CHECK(o.n==6&&o.stdout_seen&&o.stderr_seen);
    mulib_ps_destroy(&ps);
    const char *const erronly[]={"/bin/sh","-c","exec 1>&-; printf EEE >&2",NULL};
    spec=(mulib_ps_spec_t){"/bin/sh",erronly,NULL,NULL,0,0,0,0,5000,1000};
    CHECK(mulib_ps_start(&ps,&spec)==0);
    o=(output){0};CHECK(mulib_ps_wait(&ps,collect,&o)==MULIB_PS_DONE);
    CHECK(o.n==3&&o.stderr_seen&&!o.stdout_seen&&!memcmp(o.bytes,"EEE",3));
    mulib_ps_destroy(&ps);
    const char *const env_args[]={"/bin/sh","-c","printf %s \"$FOO\"",NULL};
    const char *const environ[]={"FOO=barbaz","PATH=/bin:/usr/bin",NULL};
    spec=(mulib_ps_spec_t){"/bin/sh",env_args,environ,NULL,0,0,0,0,5000,1000};
    CHECK(mulib_ps_start(&ps,&spec)==0);
    o=(output){0};CHECK(mulib_ps_wait(&ps,collect,&o)==MULIB_PS_DONE);
    CHECK(o.n==6&&!memcmp(o.bytes,"barbaz",6));
    mulib_ps_destroy(&ps);
    const char *const stdin_args[]={"/bin/cat",NULL};
    spec=(mulib_ps_spec_t){"/bin/cat",stdin_args,NULL,NULL,1,0,0,0,5000,100000};
    CHECK(mulib_ps_start(&ps,&spec)==0);
    size_t wrote=0;CHECK(mulib_ps_write(&ps,"echo",4,&wrote)==0&&wrote==4);
    CHECK(mulib_ps_close_input(&ps)==0);
    o=(output){0};CHECK(mulib_ps_wait(&ps,collect,&o)==MULIB_PS_DONE);
    CHECK(o.n==4&&!memcmp(o.bytes,"echo",4));mulib_ps_destroy(&ps);
    const char *const bad_args[]={"does-not-exist",NULL};
    spec=(mulib_ps_spec_t){"/nonexistent/executable",bad_args,NULL,NULL,0,0,0,0,5000,1000};
    CHECK(mulib_ps_start(&ps,&spec)==MULIB_PS_SYSTEM);
    mulib_ps_destroy(&ps);
    const char *const forever[]={"/bin/sh","-c","sleep 2",NULL};
    spec=(mulib_ps_spec_t){"/bin/sh",forever,NULL,NULL,0,0,0,0,120,1000};
    CHECK(mulib_ps_start(&ps,&spec)==0);
    CHECK(mulib_ps_wait(&ps,NULL,NULL)==MULIB_PS_TIMEOUT);
    mulib_ps_destroy(&ps);
    const char *const lots[]={"/bin/sh","-c","printf 123456789",NULL};
    spec=(mulib_ps_spec_t){"/bin/sh",lots,NULL,NULL,0,0,0,0,1000,4};
    CHECK(mulib_ps_start(&ps,&spec)==0);
    CHECK(mulib_ps_wait(&ps,NULL,NULL)==MULIB_PS_LIMIT);
    mulib_ps_destroy(&ps);
    const char *const ttyargs[]={"/bin/sh","-c","printf tty-okay",NULL};
    spec=(mulib_ps_spec_t){"/bin/sh",ttyargs,NULL,NULL,1,1,24,80,5000,1000};
    CHECK(mulib_ps_start(&ps,&spec)==0);
    o=(output){0};CHECK(mulib_ps_wait(&ps,collect,&o)==MULIB_PS_DONE);
    CHECK(o.n==8&&!memcmp(o.bytes,"tty-okay",8));
    mulib_ps_destroy(&ps);
}
static void test_fs(void){
    size_t len=123;
    char path[200];snprintf(path,sizeof path,"%s/fs",root_path);
    CHECK(mulib_fs_read(path,&len)==NULL && len==123);
    CHECK(mulib_fs_read_bounded(path,4,&len)==NULL && len==0);
    CHECK(mulib_fs_write(path,NULL,0)==1);
    unsigned char *b=mulib_fs_read_bounded(path,0,&len);
    CHECK(b && len==0 && b[0]==0);free(b);
    CHECK(mulib_fs_write(path,"abcd",4)==1);
    CHECK(mulib_fs_read_bounded(path,3,&len)==NULL&&len==0);
    b=mulib_fs_read_bounded(path,4,&len);
    CHECK(b&&len==4&&!memcmp(b,"abcd",5));free(b);
}
static void remove_tree(int fd){
    DIR *dir=fdopendir(fd);
    CHECK(dir!=NULL);
    struct dirent *entry;
    for(;;){
        errno=0;entry=readdir(dir);
        if(!entry){CHECK(errno==0);break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        struct stat st;
        CHECK(fstatat(fd,entry->d_name,&st,AT_SYMLINK_NOFOLLOW)==0);
        if(S_ISDIR(st.st_mode)){
            int child=openat(fd,entry->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
            CHECK(child>=0);
            remove_tree(child);
        }
        CHECK(unlinkat(fd,entry->d_name,S_ISDIR(st.st_mode)?AT_REMOVEDIR:0)==0);
    }
    CHECK(closedir(dir)==0);
}
int main(void){
    strcpy(root_path,"/tmp/mulib-harness-test-XXXXXX");
    CHECK(mkdtemp(root_path)!=NULL);
    CHECK(mulib_wfs_open(&ws,root_path)==0);
    test_wfs();test_unidiff();test_edits();test_fs();test_process();
    int cleanup=dup(ws.root_fd);
    CHECK(cleanup>=0);
    remove_tree(cleanup);
    mulib_wfs_close(&ws);
    CHECK(rmdir(root_path)==0);
    printf("PASS: workspace FS, diff, edits, process, PTY and fs\n");
    return 0;
}
