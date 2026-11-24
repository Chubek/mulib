/**
 * @file mulib-sqlite-pager.h
 * @brief SQLite main-file snapshots, POSIX locks and rollback-journal commits.
 * Define MULIB_IMPL_SQLITE_PAGER; implement SQLITE_FORMAT, BUFFER and ENDIAN
 * explicitly. No SQL parser or external SQLite dependency.
 */
#ifndef MULIB_SQLITE_PAGER_H
#define MULIB_SQLITE_PAGER_H
#include "mulib-sqlite-format.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sqlite_pager mulib_sqlite_pager_t;
/** Open flags. A NULL path or :memory: selects memory storage. */
enum { MULIB_SQLITE_OPEN_READONLY=1, MULIB_SQLITE_OPEN_CREATE=2 };
/** Open and recover a bounded snapshot. File locks are held until close.
 * POSIX file access only; other platforms support memory databases.
 * One handle per file per process; do not cohost other database libraries on
 * that file in the same process. max_bytes=0 selects 64 MiB. */
int mulib_sqlite_pager_open(const char *, unsigned flags, size_t max_bytes, mulib_sqlite_pager_t **);
/** Borrow the current image until commit/close. Zero bytes means a new database. */
const void *mulib_sqlite_pager_data(const mulib_sqlite_pager_t *, size_t *length);
/** Commit a full format-3 image using a compatible DELETE rollback journal.
 * UNCERTAIN means committed but directory durability could not be confirmed.
 * I/O failures poison the pager; close and reopen for recovery. */
int mulib_sqlite_pager_commit(mulib_sqlite_pager_t *, const void *, size_t);
/** Release the snapshot, native file handle and locks. Never implicitly commits. */
void mulib_sqlite_pager_close(mulib_sqlite_pager_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SQLITE_PAGER) && !defined(MULIB_SQLITE_PAGER_IMPLEMENTED)
#define MULIB_SQLITE_PAGER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdatomic.h>
#define MULIB_SQLITE_PAGER_POSIX_ 1
/* POSIX declarations can be hidden by strict C11 feature-test settings. */
extern int ftruncate(int, off_t);
#endif
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_sqlite_pager {
  mulib_buffer_t image; size_t limit; unsigned flags; int failed;
#ifdef MULIB_SQLITE_PAGER_POSIX_
  int fd; char *journal,*directory; dev_t device; ino_t inode;
  struct mulib_sqlite_pager *next;
#endif
};
#ifdef MULIB_SQLITE_PAGER_POSIX_
static atomic_flag mulib_sqlite_pager_mutex_=ATOMIC_FLAG_INIT;
static mulib_sqlite_pager_t *mulib_sqlite_pager_files_;
static void mulib_sqlite_pager_enter_(void) { while (atomic_flag_test_and_set(&mulib_sqlite_pager_mutex_)) {} }
static void mulib_sqlite_pager_leave_(void) { atomic_flag_clear(&mulib_sqlite_pager_mutex_); }
static const unsigned char mulib_sqlite_pager_magic_[8]={0xd9,0xd5,5,0xf9,0x20,0xa1,0x63,0xd7};
static int mulib_sqlite_pager_io_(int fd,void *data,size_t n,uint64_t offset,int writing) {
  unsigned char *p=(unsigned char *)data; ssize_t got; size_t take;
  if (offset>INT64_MAX || (uint64_t)(off_t)offset!=offset || lseek(fd,(off_t)offset,SEEK_SET)<0) return MULIB_SQLITE_IO;
  while (n) {
    take=n>INT_MAX?INT_MAX:n;
    got=writing?write(fd,p,take):read(fd,p,take);
    if (got<0 && errno==EINTR) continue;
    if (got<=0) return MULIB_SQLITE_IO;
    n-=(size_t)got; p+=got;
  }
  return 0;
}
static int mulib_sqlite_pager_directory_(const mulib_sqlite_pager_t *p) {
  int fd=open(p->directory,O_RDONLY),rc;
  if (fd<0) return MULIB_SQLITE_IO;
  rc=fsync(fd); if (close(fd)<0) rc=-1; return rc?MULIB_SQLITE_IO:0;
}
static uint32_t mulib_sqlite_pager_checksum_(const unsigned char *p,uint32_t ps,uint32_t nonce) {
  int i; for (i=(int)ps-200;i>=0;i-=200) nonce+=p[i]; return nonce;
}
static int mulib_sqlite_pager_recover_(mulib_sqlite_pager_t *p) {
  int fd,rc=0; struct stat st; mulib_buffer_t journal={0};
  unsigned char *h,*record; size_t pos,records,i,sector,ps=0,original=0,k;
  uint32_t nonce,pages,page; int valid=0;
  fd=open(p->journal,O_RDONLY);
  if (fd<0) return errno==ENOENT?0:MULIB_SQLITE_IO;
  if (fstat(fd,&st)<0 || st.st_size<0 || (uint64_t)st.st_size>p->limit+p->limit/64+65536) { rc=MULIB_SQLITE_TOOBIG; goto done; }
  if (!st.st_size) goto cold;
  journal.length=journal.capacity=(size_t)st.st_size;
  journal.data=(unsigned char *)malloc(journal.length); if (!journal.data) { rc=MULIB_SQLITE_NOMEM; goto done; }
  rc=mulib_sqlite_pager_io_(fd,journal.data,journal.length,0,0); if (rc) goto done;
  if (journal.length<28 || memcmp(journal.data,mulib_sqlite_pager_magic_,8)) goto cold;
  if (p->flags&MULIB_SQLITE_OPEN_READONLY) { rc=MULIB_SQLITE_READONLY; goto done; }
  pos=0;
  /* Validate every segment before changing a single byte of the main file. */
  while (pos+28<=journal.length) {
    h=journal.data+pos;
    if (memcmp(h,mulib_sqlite_pager_magic_,8)) { rc=MULIB_SQLITE_UNSUPPORTED; goto done; }
    pages=mulib_endian_load_be32(h+8); nonce=mulib_endian_load_be32(h+12);
    sector=mulib_endian_load_be32(h+20); k=mulib_endian_load_be32(h+24);
    if (sector<512 || sector>65536 || (sector&(sector-1)) || k<512 || k>65536 || (k&(k-1))) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    if (!ps) { ps=k; original=mulib_endian_load_be32(h+16); }
    if (k!=ps || original!=mulib_endian_load_be32(h+16) || original>p->limit/ps || sector>journal.length-pos) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    pos+=sector; records=pages==UINT32_MAX?(journal.length-pos)/(ps+8):pages;
    if (records>(journal.length-pos)/(ps+8)) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    for (i=0;i<records;++i) {
      record=journal.data+pos+i*(ps+8); page=mulib_endian_load_be32(record);
      if (!page || page>original || mulib_sqlite_pager_checksum_(record+4,(uint32_t)ps,nonce)!=mulib_endian_load_be32(record+4+ps)) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    }
    valid=1; pos+=records*(ps+8);
    if (pages==UINT32_MAX || pos==journal.length) break;
    k=(sector-pos%sector)%sector;
    if (k>journal.length-pos) { rc=MULIB_SQLITE_UNSUPPORTED; goto done; }
    pos+=k; if (pos==journal.length) break;
  }
  if (!valid || pos!=journal.length) { rc=MULIB_SQLITE_UNSUPPORTED; goto done; }
  pos=0;
  while (pos<journal.length) {
    h=journal.data+pos; sector=mulib_endian_load_be32(h+20); pages=mulib_endian_load_be32(h+8); pos+=sector;
    records=pages==UINT32_MAX?(journal.length-pos)/(ps+8):pages;
    for (i=0;i<records;++i) {
      record=journal.data+pos+i*(ps+8); page=mulib_endian_load_be32(record);
      rc=mulib_sqlite_pager_io_(p->fd,record+4,ps,(uint64_t)(page-1)*ps,1); if (rc) goto done;
    }
    pos+=records*(ps+8); if (pos==journal.length || pages==UINT32_MAX) break;
    pos+=(sector-pos%sector)%sector;
  }
  if (ftruncate(p->fd,(off_t)(original*ps))<0 || fsync(p->fd)<0) { rc=MULIB_SQLITE_IO; goto done; }
cold:
  /* Cold journals are safe to leave for read-only handles. */
  if (!(p->flags&MULIB_SQLITE_OPEN_READONLY)) {
    if (unlink(p->journal)<0 && errno!=ENOENT) rc=MULIB_SQLITE_IO;
    else rc=mulib_sqlite_pager_directory_(p);
  }
done: close(fd); mulib_buffer_free(&journal); return rc;
}
#endif
int mulib_sqlite_pager_open(const char *path,unsigned flags,size_t limit,mulib_sqlite_pager_t **out) {
  mulib_sqlite_pager_t *p; int rc=0;
  if (!out || (flags&~3u) || ((flags&1) && (flags&2))) return MULIB_SQLITE_MISUSE;
  if (!limit) limit=64u*1024u*1024u;
  if (limit>SIZE_MAX/2-65536 || limit<512) return MULIB_SQLITE_MISUSE;
  p=(mulib_sqlite_pager_t *)calloc(1,sizeof *p); if (!p) return MULIB_SQLITE_NOMEM;
  p->limit=limit; p->flags=flags;
#ifdef MULIB_SQLITE_PAGER_POSIX_
  p->fd=-1;
#endif
  if (!path || !strcmp(path,":memory:")) { *out=p; return 0; }
#ifdef MULIB_SQLITE_PAGER_POSIX_
  {
    struct stat st; struct flock lock; size_t n=strlen(path),dir; const char *slash=strrchr(path,'/');
    mulib_sqlite_pager_t *other; int registered=0;
    if (n>SIZE_MAX-9) { rc=MULIB_SQLITE_TOOBIG; goto fail; }
    p->journal=(char *)malloc(n+9); dir=slash?(size_t)(slash-path):1;
    p->directory=(char *)malloc(dir+2);
    if (!p->journal || !p->directory) { rc=MULIB_SQLITE_NOMEM; goto fail; }
    memcpy(p->journal,path,n); memcpy(p->journal+n,"-journal",9);
    if (slash) { if (dir) memcpy(p->directory,path,dir); else p->directory[dir++]='/'; }
    else p->directory[0]='.';
    p->directory[dir]=0;
    mulib_sqlite_pager_enter_();
    if (!stat(path,&st)) for (other=mulib_sqlite_pager_files_;other;other=other->next)
      if (other->device==st.st_dev && other->inode==st.st_ino) { rc=MULIB_SQLITE_BUSY; break; }
    if (rc) { mulib_sqlite_pager_leave_(); goto fail; }
    p->fd=open(path,(flags&1)?O_RDONLY:O_RDWR|((flags&2)?O_CREAT:0),0600);
    if (p->fd<0) rc=MULIB_SQLITE_CANTOPEN;
    else if (fstat(p->fd,&st)<0 || !S_ISREG(st.st_mode) || st.st_size<0) rc=MULIB_SQLITE_IO;
    else {
      p->device=st.st_dev; p->inode=st.st_ino;
      p->next=mulib_sqlite_pager_files_; mulib_sqlite_pager_files_=p; registered=1;
    }
    mulib_sqlite_pager_leave_();
    if (rc) goto fail;
    (void)registered;
    memset(&lock,0,sizeof lock); lock.l_type=(flags&1)?F_RDLCK:F_WRLCK;
    lock.l_whence=SEEK_SET; lock.l_start=(off_t)0x40000000; lock.l_len=512;
    if (fcntl(p->fd,F_SETLK,&lock)<0) { rc=(errno==EACCES || errno==EAGAIN)?MULIB_SQLITE_BUSY:MULIB_SQLITE_IO; goto fail; }
    /* WAL state is never ignored, even if the main file looks consistent. */
    { char *wal=(char *)malloc(n+5); if (!wal) { rc=MULIB_SQLITE_NOMEM; goto fail; }
      memcpy(wal,path,n); memcpy(wal+n,"-wal",5);
      if (!stat(wal,&st) && st.st_size>0) rc=MULIB_SQLITE_UNSUPPORTED;
      else if (errno!=ENOENT && stat(wal,&st)<0) rc=MULIB_SQLITE_IO;
      free(wal); if (rc) goto fail;
    }
    rc=mulib_sqlite_pager_recover_(p); if (rc) goto fail;
    if (fstat(p->fd,&st)<0) { rc=MULIB_SQLITE_IO; goto fail; }
    if ((uint64_t)st.st_size>limit) { rc=MULIB_SQLITE_TOOBIG; goto fail; }
    if (st.st_size) {
      p->image.length=p->image.capacity=(size_t)st.st_size;
      p->image.data=(unsigned char *)malloc(p->image.length); if (!p->image.data) { rc=MULIB_SQLITE_NOMEM; goto fail; }
      rc=mulib_sqlite_pager_io_(p->fd,p->image.data,p->image.length,0,0); if (rc) goto fail;
      if (p->image.length>=20 && (p->image.data[18]==2 || p->image.data[19]==2)) { rc=MULIB_SQLITE_UNSUPPORTED; goto fail; }
    }
  }
  *out=p; return 0;
#else
  rc=MULIB_SQLITE_UNSUPPORTED;
#endif
fail: mulib_sqlite_pager_close(p); return rc;
}
const void *mulib_sqlite_pager_data(const mulib_sqlite_pager_t *p,size_t *n) {
  if (n) *n=p?p->image.length:0;
  return p?p->image.data:NULL;
}
int mulib_sqlite_pager_commit(mulib_sqlite_pager_t *p,const void *bytes,size_t n) {
  mulib_buffer_t copy={0}; mulib_sqlite_format_t f; int rc;
  if (!p || (!bytes && n)) return MULIB_SQLITE_MISUSE;
  if (p->flags&1) return MULIB_SQLITE_READONLY;
  if (p->failed) return MULIB_SQLITE_IO;
  if (n>p->limit) return MULIB_SQLITE_TOOBIG;
  rc=mulib_sqlite_format_open(&f,bytes,n); if (rc) return rc;
  if (f.data[18]!=1 || f.data[19]!=1) return MULIB_SQLITE_UNSUPPORTED;
  if (p->image.length) {
    mulib_sqlite_format_t old;
    rc=mulib_sqlite_format_open(&old,p->image.data,p->image.length); if (rc) return rc;
    if (old.page_size!=f.page_size || old.data[18]!=1 || old.data[19]!=1) return MULIB_SQLITE_UNSUPPORTED;
  }
  if (!mulib_buffer_append(&copy,bytes,n)) return MULIB_SQLITE_NOMEM;
#ifdef MULIB_SQLITE_PAGER_POSIX_
  if (p->fd>=0) {
    mulib_buffer_t journal={0}; unsigned char header[512]={0},word[4];
    uint32_t ps=f.page_size,nonce=0,page,pages; int fd=-1,randomfd,started=0;
    if (p->image.length && (p->image.length%ps || p->image.length/ps>UINT32_MAX-1)) { rc=MULIB_SQLITE_UNSUPPORTED; goto disk_done; }
    pages=(uint32_t)(p->image.length/ps);
    randomfd=open("/dev/urandom",O_RDONLY);
    if (randomfd<0) { rc=MULIB_SQLITE_IO; goto disk_done; }
    rc=mulib_sqlite_pager_io_(randomfd,&nonce,4,0,0); close(randomfd);
    /* /dev/urandom is not seekable on every POSIX backend. */
    if (rc) goto disk_done;
    mulib_endian_store_be32(header+8,pages); mulib_endian_store_be32(header+12,nonce);
    mulib_endian_store_be32(header+16,pages); mulib_endian_store_be32(header+20,512); mulib_endian_store_be32(header+24,ps);
    if (!mulib_buffer_append(&journal,header,512)) { rc=MULIB_SQLITE_NOMEM; goto disk_done; }
    for (page=1;page<=pages;++page) {
      const unsigned char *old=p->image.data+(size_t)(page-1)*ps;
      mulib_endian_store_be32(word,page);
      if (!mulib_buffer_append(&journal,word,4) || !mulib_buffer_append(&journal,old,ps)) { rc=MULIB_SQLITE_NOMEM; goto disk_done; }
      mulib_endian_store_be32(word,mulib_sqlite_pager_checksum_(old,ps,nonce));
      if (!mulib_buffer_append(&journal,word,4)) { rc=MULIB_SQLITE_NOMEM; goto disk_done; }
    }
    fd=open(p->journal,O_WRONLY|O_CREAT|O_EXCL,0600);
    if (fd<0) { rc=MULIB_SQLITE_IO; goto disk_done; }
    rc=mulib_sqlite_pager_io_(fd,journal.data,journal.length,0,1);
    if (!rc && fsync(fd)<0) rc=MULIB_SQLITE_IO;
    if (!rc) rc=mulib_sqlite_pager_io_(fd,(void *)mulib_sqlite_pager_magic_,8,0,1);
    if (!rc && fsync(fd)<0) rc=MULIB_SQLITE_IO;
    if (!rc) rc=mulib_sqlite_pager_directory_(p);
    if (rc) goto disk_done;
    started=1;
    rc=mulib_sqlite_pager_io_(p->fd,copy.data,n,0,1);
    if (!rc && ftruncate(p->fd,(off_t)n)<0) rc=MULIB_SQLITE_IO;
    if (!rc && fsync(p->fd)<0) rc=MULIB_SQLITE_IO;
    if (!rc && unlink(p->journal)<0) rc=MULIB_SQLITE_IO;
    if (!rc && mulib_sqlite_pager_directory_(p)) rc=MULIB_SQLITE_UNCERTAIN;
disk_done:
    if (fd>=0) close(fd);
    mulib_buffer_free(&journal);
    if (rc && rc!=MULIB_SQLITE_UNCERTAIN) {
      /* Never remove a potentially needed journal after a failed write/sync. */
      if (started || fd>=0) p->failed=1;
      mulib_buffer_free(&copy); return rc;
    }
  } else rc=0;
#else
  rc=0;
#endif
  mulib_buffer_free(&p->image); p->image=copy; return rc;
}
void mulib_sqlite_pager_close(mulib_sqlite_pager_t *p) {
  if (!p) return;
#ifdef MULIB_SQLITE_PAGER_POSIX_
  if (p->fd>=0) {
    mulib_sqlite_pager_t **link;
    mulib_sqlite_pager_enter_();
    for (link=&mulib_sqlite_pager_files_;*link && *link!=p;link=&(*link)->next) {}
    if (*link) *link=p->next;
    close(p->fd); mulib_sqlite_pager_leave_();
  }
  free(p->journal); free(p->directory);
#endif
  mulib_buffer_free(&p->image); free(p);
}
#ifdef __cplusplus
}
#endif
#endif
