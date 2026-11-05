#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_SQLITE_FORMAT
#include "mulib-sqlite-format.h"
#include <assert.h>

static int row(void *user,int64_t rowid,const void *record,size_t length) {
  size_t count=0; mulib_sqlite_value_t *v=NULL; int *seen=(int *)user;
  assert(rowid==42);
  assert(!mulib_sqlite_format_decode(record,length,0,&v,&count));
  assert(count==1 && v[0].integer==INT64_MAX); ++*seen;
  mulib_sqlite_format_values_free(v,count); return 0;
}
int main(void) {
  mulib_sqlite_format_builder_t builder;
  mulib_sqlite_format_t image;
  mulib_sqlite_value_t v={0}; mulib_buffer_t record={0};
  mulib_sqlite_format_row_t r; int seen=0;
  v.type=MULIB_SQLITE_INTEGER; v.integer=INT64_MAX;
  assert(!mulib_sqlite_format_encode(&v,1,&record));
  assert(!mulib_sqlite_format_builder_init(&builder,512,0));
  r.rowid=42; r.record=record.data; r.length=record.length;
  assert(!mulib_sqlite_format_table(&builder,1,&r,1));
  mulib_sqlite_format_finish(&builder);
  assert(!mulib_sqlite_format_open(&image,builder.image.data,builder.image.length));
  assert(!mulib_sqlite_format_scan(&image,1,0,row,&seen) && seen==1);
  mulib_buffer_free(&record); mulib_sqlite_format_builder_free(&builder); return 0;
}
