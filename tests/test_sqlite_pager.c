#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
extern int ftruncate(int,off_t);
static size_t operations,fail_at;
static int fault(void) { if (++operations==fail_at) { errno=EIO; return 1; } return 0; }
static ssize_t checked_write(int fd,const void *p,size_t n) { return fault()?-1:write(fd,p,n); }
static int checked_sync(int fd) { return fault()?-1:fsync(fd); }
static int checked_truncate(int fd,off_t n) { return fault()?-1:ftruncate(fd,n); }
static int checked_unlink(const char *p) { return fault()?-1:unlink(p); }
#define write checked_write
#define fsync checked_sync
#define ftruncate checked_truncate
#define unlink checked_unlink
#endif
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_SQLITE_FORMAT
#define MULIB_IMPL_SQLITE_PAGER
#include "mulib-sqlite-pager.h"
#if defined(__unix__) || defined(__APPLE__)
#undef write
#undef fsync
#undef ftruncate
#undef unlink
static void write_file(const char *path,const void *data,size_t length) {
  FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(data,1,length,f)==length); assert(!fclose(f));
}
#endif
int main(void) {
#if defined(__unix__) || defined(__APPLE__)
  mulib_sqlite_format_builder_t old,newer; mulib_sqlite_pager_t *p=NULL,*other=NULL;
  const char *path="sqlite-pager-test.db"; const void *image; size_t i,count,length; int rc;
  assert(!mulib_sqlite_format_builder_init(&old,512,0));
  assert(!mulib_sqlite_format_table(&old,1,NULL,0)); mulib_sqlite_format_finish(&old);
  assert(!mulib_sqlite_format_builder_init(&newer,512,0));
  assert(!mulib_sqlite_format_table(&newer,1,NULL,0));
  mulib_endian_store_be32(newer.image.data+60,42); mulib_sqlite_format_finish(&newer);
  write_file(path,old.image.data,old.image.length);
  assert(!mulib_sqlite_pager_open(path,0,0,&p));
  assert(mulib_sqlite_pager_open(path,0,0,&other)==MULIB_SQLITE_BUSY && !other);
  operations=0; assert(!mulib_sqlite_pager_commit(p,newer.image.data,newer.image.length)); count=operations;
  mulib_sqlite_pager_close(p);
  for (i=1;i<=count;++i) {
    assert(!remove(path)); write_file(path,old.image.data,old.image.length);
    assert(!mulib_sqlite_pager_open(path,0,0,&p)); operations=0; fail_at=i;
    rc=mulib_sqlite_pager_commit(p,newer.image.data,newer.image.length); fail_at=0;
    assert(rc==MULIB_SQLITE_IO || rc==MULIB_SQLITE_UNCERTAIN);
    mulib_sqlite_pager_close(p);
    assert(!mulib_sqlite_pager_open(path,0,0,&p));
    image=mulib_sqlite_pager_data(p,&length); assert(length==512);
    assert(!memcmp(image,rc==MULIB_SQLITE_UNCERTAIN?newer.image.data:old.image.data,512));
    mulib_sqlite_pager_close(p);
  }
  assert(!remove(path));
  mulib_sqlite_format_builder_free(&old); mulib_sqlite_format_builder_free(&newer);
  puts("SQLite pager I/O fault, rollback recovery and lock tests passed");
#else
  puts("SQLite POSIX pager fault tests skipped on this platform");
#endif
  return 0;
}
