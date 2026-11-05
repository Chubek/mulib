#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static size_t allocations,fail_at,live;
static int persistent;
static int allocation_fails(void) { ++allocations; return fail_at && (persistent?allocations>=fail_at:allocations==fail_at); }
static void *checked_malloc(size_t n) {
  void *p; if (allocation_fails()) return NULL;
  p=malloc(n); if (p) ++live; return p;
}
static void *checked_calloc(size_t n,size_t size) {
  void *p; assert(!size || n<=SIZE_MAX/size); p=checked_malloc(n*size); if (p) memset(p,0,n*size); return p;
}
static void *checked_realloc(void *old,size_t n) {
  void *p; int existed=old!=NULL; assert(n); if (allocation_fails()) return NULL;
  p=realloc(old,n); if (p && !existed) ++live; return p;
}
static void checked_free(void *p) { if (p) { assert(live); --live; } free(p); }
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PEG
#define MULIB_IMPL_RNG
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SQLITE_FORMAT
#define MULIB_IMPL_SQLITE_PAGER
#define MULIB_IMPL_SQLITE
#include "mulib-sqlite.h"
#undef malloc
#undef calloc
#undef realloc
#undef free

static void sweep_statement(mulib_sqlite_t *db,const char *sql) {
  mulib_sqlite_stmt_t *s=NULL; size_t i,count,baseline; int rc;
  assert(!mulib_sqlite_prepare(db,sql,strlen(sql),&s,NULL,NULL));
  assert(!mulib_sqlite_exec(db,"BEGIN;",NULL));
  allocations=0; assert(mulib_sqlite_step(s)==MULIB_SQLITE_DONE); count=allocations;
  assert(!mulib_sqlite_reset(s));
  /* Repeat in a transaction and roll it back after each operation so rowid
   * uniqueness and schema state remain identical at each failure point. */
  assert(!mulib_sqlite_exec(db,"ROLLBACK; BEGIN;",NULL));
  baseline=live;
  for (i=1;i<=count;++i) {
    allocations=0; fail_at=i;
    rc=mulib_sqlite_step(s);
    fail_at=0;
    if (rc!=MULIB_SQLITE_NOMEM) fprintf(stderr,"step allocation %zu/%zu: %d\n",i,count,rc);
    assert(rc==MULIB_SQLITE_NOMEM);
    assert(mulib_sqlite_reset(s)==MULIB_SQLITE_NOMEM);
    assert(live==baseline);
  }
  assert(!mulib_sqlite_exec(db,"ROLLBACK;",NULL));
  mulib_sqlite_finalize(s);
}
static void sweep_query(mulib_sqlite_t *db) {
  const char *sql="SELECT DISTINCT upper(value),id+1 AS nextid FROM t WHERE id>0 ORDER BY nextid DESC LIMIT 1;";
  mulib_sqlite_stmt_t *s=NULL; size_t count,i,baseline; int rc;
  assert(!mulib_sqlite_exec(db,"INSERT INTO t VALUES(1,'one'),(2,'two');",NULL));
  assert(!mulib_sqlite_prepare(db,sql,strlen(sql),&s,NULL,NULL)); baseline=live;
  allocations=0; assert(mulib_sqlite_step(s)==MULIB_SQLITE_ROW); count=allocations; assert(!mulib_sqlite_reset(s));
  for (i=1;i<=count;++i) {
    allocations=0; fail_at=i; rc=mulib_sqlite_step(s); fail_at=0;
    assert(rc==MULIB_SQLITE_NOMEM); assert(mulib_sqlite_reset(s)==MULIB_SQLITE_NOMEM); assert(live==baseline);
  }
  mulib_sqlite_finalize(s); assert(!mulib_sqlite_exec(db,"DELETE FROM t;",NULL));
}
int main(void) {
  mulib_sqlite_t *db=NULL; mulib_sqlite_error_t error; size_t i,count,baseline; int rc;
  allocations=0; assert(!mulib_sqlite_open(":memory:",0,NULL,&db,&error)); count=allocations;
  assert(!mulib_sqlite_close(db) && !live);
  for (i=1;i<=count;++i) {
    allocations=0; fail_at=i; db=NULL;
    rc=mulib_sqlite_open(":memory:",0,NULL,&db,&error);
    fail_at=0;
    if (rc!=MULIB_SQLITE_NOMEM) fprintf(stderr,"open allocation %zu/%zu: %d\n",i,count,rc);
    assert(rc==MULIB_SQLITE_NOMEM && !db && !live);
  }
  assert(!mulib_sqlite_open(":memory:",0,NULL,&db,&error));
  assert(!mulib_sqlite_exec(db,"CREATE TABLE t(id INTEGER PRIMARY KEY, value TEXT);",NULL));
  {
    const char *sql="SELECT ?1, 'value', 1+2;"; mulib_sqlite_stmt_t *s=NULL;
    allocations=0; assert(!mulib_sqlite_prepare(db,sql,strlen(sql),&s,NULL,NULL)); count=allocations;
    mulib_sqlite_finalize(s); baseline=live;
    for (i=1;i<=count;++i) {
      allocations=0; fail_at=i; s=NULL;
      rc=mulib_sqlite_prepare(db,sql,strlen(sql),&s,NULL,&error); fail_at=0;
      assert(rc==MULIB_SQLITE_NOMEM && !s && live==baseline);
    }
  }
  sweep_statement(db,"INSERT INTO t VALUES(42,'value');");
  persistent=1; sweep_statement(db,"INSERT INTO t VALUES(42,'value');");
  sweep_query(db); persistent=0; sweep_query(db);
  assert(!mulib_sqlite_close(db) && !live);
  puts("SQLite allocation failure sweeps passed"); return 0;
}
