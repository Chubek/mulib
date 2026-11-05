#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_SQLITE_FORMAT
#include "mulib-sqlite-format.h"
#include <assert.h>

static size_t seen;
static int verify(void *u,int64_t id,const void *p,size_t n) {
  mulib_sqlite_value_t *v=NULL; size_t count; (void)u;
  assert(id==(int64_t)seen-100);
  assert(!mulib_sqlite_format_decode(p,n,0,&v,&count));
  assert(count==2 && v[0].integer==id && v[1].length==7000);
  mulib_sqlite_format_values_free(v,count); ++seen; return 0;
}
static int abort_row(void *u,int64_t id,const void *p,size_t n) { (void)u; (void)id; (void)p; (void)n; return MULIB_SQLITE_ABORT; }
int main(void) {
  uint64_t numbers[]={0,127,128,16383,16384,UINT64_C(0xffffffffffffff),UINT64_C(0x100000000000000),UINT64_MAX};
  int64_t signed_numbers[]={INT64_MIN,-140737488355329LL,-8388609,-32769,-129,-1,0,1,127,128,32768,8388608,INT64_MAX};
  unsigned char bytes[9],*payload; size_t i,j,n,used,count;
  mulib_sqlite_value_t v[2]={{0}},*decoded=NULL; mulib_buffer_t record={0};
  for (i=0;i<sizeof numbers/sizeof *numbers;++i) { uint64_t result=42; n=mulib_sqlite_format_put_varint(numbers[i],bytes); assert(!mulib_sqlite_format_varint(bytes,n,&result,&used) && result==numbers[i] && used==n); if (n>1) assert(mulib_sqlite_format_varint(bytes,n-1,&result,&used)==MULIB_SQLITE_CORRUPT); }
  for (i=0;i<sizeof signed_numbers/sizeof *signed_numbers;++i) {
    v[0].type=MULIB_SQLITE_INTEGER; v[0].integer=signed_numbers[i];
    assert(!mulib_sqlite_format_encode(v,1,&record)); assert(!mulib_sqlite_format_decode(record.data,record.length,0,&decoded,&count));
    assert(count==1 && decoded[0].integer==signed_numbers[i]); mulib_sqlite_format_values_free(decoded,count); record.length=0;
  }
  payload=(unsigned char *)malloc(7000); assert(payload); memset(payload,0xa5,7000);
  assert(!mulib_sqlite_value_bytes(&v[1],MULIB_SQLITE_BLOB,payload,7000)); free(payload);
  for (j=512;j<=65536;j*=2) {
    mulib_sqlite_format_builder_t builder; mulib_sqlite_format_t f;
    mulib_sqlite_format_row_t rows[400]; mulib_buffer_t records[400]={{0}};
    assert(!mulib_sqlite_format_builder_init(&builder,(uint32_t)j,16u*1024u*1024u));
    for (i=0;i<400;++i) { v[0].integer=(int64_t)i-100; assert(!mulib_sqlite_format_encode(v,2,&records[i])); rows[i].rowid=v[0].integer; rows[i].record=records[i].data; rows[i].length=records[i].length; }
    assert(!mulib_sqlite_format_table(&builder,1,rows,400)); mulib_sqlite_format_finish(&builder);
    assert(!mulib_sqlite_format_open(&f,builder.image.data,builder.image.length)); seen=0;
    assert(!mulib_sqlite_format_scan(&f,1,10000,verify,NULL) && seen==400);
    for (i=0;i<400;++i) mulib_buffer_free(&records[i]);
    mulib_sqlite_format_builder_free(&builder);
  }
  mulib_sqlite_value_clear(&v[1]); mulib_buffer_free(&record);
  {
    mulib_sqlite_format_builder_t b; mulib_sqlite_format_t f; mulib_sqlite_format_row_t row;
    v[0].integer=42; assert(!mulib_sqlite_format_encode(v,1,&record));
    assert(!mulib_sqlite_format_builder_init(&b,512,0)); row.rowid=1; row.record=record.data; row.length=record.length;
    assert(!mulib_sqlite_format_table(&b,1,&row,1)); mulib_sqlite_format_finish(&b);
    assert(!mulib_sqlite_format_open(&f,b.image.data,b.image.length));
    assert(mulib_sqlite_format_scan(&f,1,0,abort_row,NULL)==MULIB_SQLITE_ABORT);
    b.image.data[108]=0; b.image.data[109]=1;
    assert(mulib_sqlite_format_scan(&f,1,0,abort_row,NULL)==MULIB_SQLITE_CORRUPT);
    assert(mulib_sqlite_format_open(&f,b.image.data,99)==MULIB_SQLITE_NOTADB);
    b.image.data[19]=3; assert(mulib_sqlite_format_open(&f,b.image.data,512)==MULIB_SQLITE_UNSUPPORTED);
    mulib_sqlite_format_builder_free(&b); mulib_buffer_free(&record);
  }
  puts("SQLite records, varints, overflow and B-tree tests passed"); return 0;
}
