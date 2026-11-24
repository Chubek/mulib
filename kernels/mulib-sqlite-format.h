/**
 * @file mulib-sqlite-format.h
 * @brief Independent SQLite format-3 records and table B-tree images.
 * Define MULIB_IMPL_SQLITE_FORMAT and explicitly implement BUFFER and ENDIAN.
 * Not a SQL engine; see manual/mulib-sqlite-format.md for supported formats.
 */
#ifndef MULIB_SQLITE_FORMAT_H
#define MULIB_SQLITE_FORMAT_H
#include "mulib-buffer.h"
#include "mulib-endian.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Result codes; ROW/DONE are successful statement states, not errors. */
typedef enum mulib_sqlite_code {
  MULIB_SQLITE_OK=0, MULIB_SQLITE_ERROR=1, MULIB_SQLITE_ABORT=4,
  MULIB_SQLITE_BUSY=5, MULIB_SQLITE_NOMEM=7, MULIB_SQLITE_READONLY=8,
  MULIB_SQLITE_INTERRUPT=9, MULIB_SQLITE_IO=10, MULIB_SQLITE_CORRUPT=11,
  MULIB_SQLITE_FULL=13, MULIB_SQLITE_CANTOPEN=14, MULIB_SQLITE_SCHEMA=17,
  MULIB_SQLITE_TOOBIG=18, MULIB_SQLITE_CONSTRAINT=19, MULIB_SQLITE_MISMATCH=20,
  MULIB_SQLITE_MISUSE=21, MULIB_SQLITE_UNSUPPORTED=22, MULIB_SQLITE_RANGE=25,
  MULIB_SQLITE_NOTADB=26, MULIB_SQLITE_UNCERTAIN=27,
  MULIB_SQLITE_ROW=100, MULIB_SQLITE_DONE=101
} mulib_sqlite_code_t;
/** SQLite storage classes. TEXT/BLOB use explicit lengths, not strlen. */
typedef enum mulib_sqlite_type {
  MULIB_SQLITE_NULL, MULIB_SQLITE_INTEGER, MULIB_SQLITE_REAL,
  MULIB_SQLITE_TEXT, MULIB_SQLITE_BLOB
} mulib_sqlite_type_t;
/** Owned value. Initialize to zero; bytes are malloc-owned for TEXT/BLOB.
 * Never shallow-copy an owned byte value and free both copies. */
typedef struct mulib_sqlite_value {
  mulib_sqlite_type_t type;
  int64_t integer;
  double real;
  unsigned char *bytes;
  size_t length;
} mulib_sqlite_value_t;
/** Free a value's bytes and set it to SQL NULL. NULL is harmless. */
void mulib_sqlite_value_clear(mulib_sqlite_value_t *);
/** Copy into an initialized value; failure leaves destination unchanged. */
int mulib_sqlite_value_copy(mulib_sqlite_value_t *, const mulib_sqlite_value_t *);
/** Set copied TEXT/BLOB bytes (with a convenience trailing NUL); binary safe. */
int mulib_sqlite_value_bytes(mulib_sqlite_value_t *, mulib_sqlite_type_t, const void *, size_t);
/** Decode SQLite's big-endian 1–9-byte varint, not LEB128. */
int mulib_sqlite_format_varint(const void *, size_t, uint64_t *, size_t *);
/** Encode a SQLite varint; output must have room for nine bytes. */
size_t mulib_sqlite_format_put_varint(uint64_t, unsigned char [9]);
/** Decode an exact record into owned values. Text bytes retain database encoding.
 * Output unchanged on error. max_columns=0 selects 2000. */
int mulib_sqlite_format_decode(const void *, size_t, size_t max_columns,
    mulib_sqlite_value_t **values, size_t *count);
/** Append an encoded record; buffer contents unchanged on error. */
int mulib_sqlite_format_encode(const mulib_sqlite_value_t *, size_t, mulib_buffer_t *);
/** Release an owned decoded record. */
void mulib_sqlite_format_values_free(mulib_sqlite_value_t *, size_t);
/** Borrowed validated database image. Validation is structural, not integrity_check. */
typedef struct mulib_sqlite_format {
  const unsigned char *data;
  size_t length;
  uint32_t page_size, usable_size, page_count, encoding, schema_format;
} mulib_sqlite_format_t;
/** Validate a complete main-file image. Does not overlay WAL or recover journals. */
int mulib_sqlite_format_open(mulib_sqlite_format_t *, const void *, size_t);
/** Callback receives borrowed raw record bytes; return OK to continue. */
typedef int (*mulib_sqlite_format_row_fn)(void *, int64_t, const void *, size_t);
/** Scan a rowid table in key order, including overflow and interior pages.
 * Reject cycles, duplicate rowids and invalid bounds; depth is limited to 64. */
int mulib_sqlite_format_scan(const mulib_sqlite_format_t *, uint32_t root,
    size_t max_payload, mulib_sqlite_format_row_fn, void *);
/** One borrowed table row for image construction; records must already be encoded. */
typedef struct mulib_sqlite_format_row {
  int64_t rowid;
  const void *record;
  size_t length;
} mulib_sqlite_format_row_t;
/** Owned new-image builder. Directly writing existing files is not its job. */
typedef struct mulib_sqlite_format_builder {
  mulib_buffer_t image;
  uint32_t page_size;
  size_t max_bytes;
} mulib_sqlite_format_builder_t;
/** Initialize a fresh format-4 UTF-8 image. page_size=0 selects 4096;
 * max_bytes=0 selects 64 MiB. Header values may subsequently be customized. */
int mulib_sqlite_format_builder_init(mulib_sqlite_format_builder_t *, uint32_t page_size, size_t max_bytes);
/** Append a zero-filled page; return its number or zero on failure. */
uint32_t mulib_sqlite_format_page(mulib_sqlite_format_builder_t *);
/** Populate a reserved root with an entire sorted rowid table, allocating
 * interior/leaf/overflow pages. On error discard the builder. */
int mulib_sqlite_format_table(mulib_sqlite_format_builder_t *, uint32_t root,
    const mulib_sqlite_format_row_t *, size_t count);
/** Update header database-size/version-valid fields before publishing. */
void mulib_sqlite_format_finish(mulib_sqlite_format_builder_t *);
/** Free builder storage. */
void mulib_sqlite_format_builder_free(mulib_sqlite_format_builder_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SQLITE_FORMAT) && !defined(MULIB_SQLITE_FORMAT_IMPLEMENTED)
#define MULIB_SQLITE_FORMAT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
static uint32_t mulib_sqlite_format_u16_(const unsigned char *p) { return (uint32_t)p[0]*256+p[1]; }
static void mulib_sqlite_format_s16_(unsigned char *p, size_t v) { p[0]=(unsigned char)(v>>8); p[1]=(unsigned char)v; }
void mulib_sqlite_value_clear(mulib_sqlite_value_t *v) {
  if (v) { free(v->bytes); memset(v,0,sizeof *v); }
}
int mulib_sqlite_value_bytes(mulib_sqlite_value_t *v, mulib_sqlite_type_t type, const void *p, size_t n) {
  unsigned char *copy;
  if (!v || (type!=MULIB_SQLITE_TEXT && type!=MULIB_SQLITE_BLOB) || (!p && n)) return MULIB_SQLITE_MISUSE;
  if (n==SIZE_MAX) return MULIB_SQLITE_TOOBIG;
  copy=(unsigned char *)malloc(n+1); if (!copy) return MULIB_SQLITE_NOMEM;
  if (n) memcpy(copy,p,n);
  copy[n]=0; mulib_sqlite_value_clear(v); v->type=type; v->bytes=copy; v->length=n; return 0;
}
int mulib_sqlite_value_copy(mulib_sqlite_value_t *to, const mulib_sqlite_value_t *from) {
  if (!to || !from) return MULIB_SQLITE_MISUSE;
  if (from->type<MULIB_SQLITE_NULL || from->type>MULIB_SQLITE_BLOB) return MULIB_SQLITE_MISUSE;
  if (to==from) return 0;
  if (from->type==MULIB_SQLITE_REAL && isnan(from->real)) { mulib_sqlite_value_clear(to); return 0; }
  if (from->type==MULIB_SQLITE_TEXT || from->type==MULIB_SQLITE_BLOB)
    return mulib_sqlite_value_bytes(to,from->type,from->bytes,from->length);
  mulib_sqlite_value_clear(to); *to=*from; to->bytes=NULL; return 0;
}
int mulib_sqlite_format_varint(const void *bytes, size_t n, uint64_t *v, size_t *used) {
  const unsigned char *p=(const unsigned char *)bytes;
  size_t i; uint64_t x=0;
  if (!p || !v) return MULIB_SQLITE_MISUSE;
  for (i=0;i<n && i<9;++i) {
    if (i==8) { x=(x<<8)|p[i]; *v=x; if (used) *used=9; return 0; }
    x=(x<<7)|(p[i]&127);
    if (!(p[i]&128)) { *v=x; if (used) *used=i+1; return 0; }
  }
  return MULIB_SQLITE_CORRUPT;
}
size_t mulib_sqlite_format_put_varint(uint64_t x, unsigned char p[9]) {
  size_t n=1,i; uint64_t t=x;
  if (x>UINT64_C(0x00ffffffffffffff)) {
    p[8]=(unsigned char)x; x>>=8;
    for (i=8;i--;) { p[i]=(unsigned char)((x&127)|128); x>>=7; } return 9;
  }
  while (t>=128) { ++n; t>>=7; }
  for (i=n;i--;) { p[i]=(unsigned char)((x&127)|(i+1<n?128:0)); x>>=7; }
  return n;
}
static int64_t mulib_sqlite_format_signed_(uint64_t u) {
  return u<=INT64_MAX ? (int64_t)u : -1-(int64_t)(UINT64_MAX-u);
}
static size_t mulib_sqlite_format_serial_size_(uint64_t t) {
  static const unsigned char sizes[12]={0,1,2,3,4,6,8,8,0,0,0,0};
  return t<12 ? sizes[t] : (size_t)((t-12)/2);
}
void mulib_sqlite_format_values_free(mulib_sqlite_value_t *v, size_t n) {
  size_t i; if (v) { for (i=0;i<n;++i) mulib_sqlite_value_clear(&v[i]); free(v); }
}
int mulib_sqlite_format_decode(const void *bytes, size_t n, size_t limit, mulib_sqlite_value_t **out, size_t *count) {
  const unsigned char *p=(const unsigned char *)bytes;
  uint64_t h,t,u; size_t k,pos,body,cols=0,i,j,len; int rc;
  mulib_sqlite_value_t *values=NULL;
  if (!out || !count || !p) return MULIB_SQLITE_MISUSE;
  if (sizeof(double)!=8 || CHAR_BIT!=8) return MULIB_SQLITE_UNSUPPORTED;
  if (!limit) limit=2000;
  rc=mulib_sqlite_format_varint(p,n,&h,&k); if (rc || h<k || h>n) return MULIB_SQLITE_CORRUPT;
  pos=k;
  while (pos<(size_t)h) {
    rc=mulib_sqlite_format_varint(p+pos,(size_t)h-pos,&t,&j);
    if (rc || t==10 || t==11 || (t>=12 && (t-12)/2>SIZE_MAX)) return MULIB_SQLITE_CORRUPT;
    pos+=j; if (++cols>limit) return MULIB_SQLITE_TOOBIG;
  }
  if (cols>SIZE_MAX/sizeof *values) return MULIB_SQLITE_TOOBIG;
  values=(mulib_sqlite_value_t *)calloc(cols?cols:1,sizeof *values); if (!values) return MULIB_SQLITE_NOMEM;
  pos=k; body=(size_t)h;
  for (i=0;i<cols;++i) {
    rc=mulib_sqlite_format_varint(p+pos,(size_t)h-pos,&t,&j); if (rc) goto fail;
    pos+=j; len=mulib_sqlite_format_serial_size_(t);
    if (len>n-body) { rc=MULIB_SQLITE_CORRUPT; goto fail; }
    if (t>=12) {
      rc=mulib_sqlite_value_bytes(&values[i],(t&1)?MULIB_SQLITE_TEXT:MULIB_SQLITE_BLOB,p+body,len);
      if (rc) goto fail;
    } else if (t) {
      values[i].type=t==7?MULIB_SQLITE_REAL:MULIB_SQLITE_INTEGER;
      u=0; for (j=0;j<len;++j) u=(u<<8)|p[body+j];
      if (t==7) { memcpy(&values[i].real,&u,8); if (isnan(values[i].real)) values[i].type=MULIB_SQLITE_NULL; }
      else {
        if (len && len<8 && (p[body]&128)) u|=UINT64_MAX<<(len*8);
        values[i].integer=t==8?0:t==9?1:mulib_sqlite_format_signed_(u);
      }
    }
    body+=len;
  }
  if (body!=n) { rc=MULIB_SQLITE_CORRUPT; goto fail; }
  *out=values; *count=cols; return 0;
fail: mulib_sqlite_format_values_free(values,cols); return rc;
}
static uint64_t mulib_sqlite_format_serial_(const mulib_sqlite_value_t *v) {
  int64_t x=v->integer;
  if (v->type==MULIB_SQLITE_NULL) return 0;
  if (v->type==MULIB_SQLITE_REAL) return isnan(v->real)?0:7;
  if (v->type==MULIB_SQLITE_TEXT || v->type==MULIB_SQLITE_BLOB)
    return (uint64_t)v->length*2+(v->type==MULIB_SQLITE_TEXT?13:12);
  if (x==0 || x==1) return (uint64_t)x+8;
  if (x>=-128 && x<=127) return 1;
  if (x>=-32768 && x<=32767) return 2;
  if (x>=-8388608 && x<=8388607) return 3;
  if (x>=INT32_MIN && x<=INT32_MAX) return 4;
  if (x>=-INT64_C(140737488355328) && x<=INT64_C(140737488355327)) return 5;
  return 6;
}
int mulib_sqlite_format_encode(const mulib_sqlite_value_t *v, size_t count, mulib_buffer_t *out) {
  mulib_buffer_t header={0},body={0},record={0}; unsigned char bytes[9];
  uint64_t t,u; size_t i,j,n,h,k; int rc=MULIB_SQLITE_NOMEM;
  if ((!v && count) || !out || sizeof(double)!=8 || CHAR_BIT!=8) return MULIB_SQLITE_MISUSE;
  for (i=0;i<count;++i) {
    if (v[i].type>MULIB_SQLITE_BLOB || v[i].length>(UINT64_MAX-13)/2) { rc=MULIB_SQLITE_TOOBIG; goto done; }
    t=mulib_sqlite_format_serial_(&v[i]); n=mulib_sqlite_format_put_varint(t,bytes);
    if (!mulib_buffer_append(&header,bytes,n)) goto done;
    n=mulib_sqlite_format_serial_size_(t);
    if (t>=12) { if (!mulib_buffer_append(&body,v[i].bytes,n)) goto done; }
    else {
      if (t==7) memcpy(&u,&v[i].real,8); else u=(uint64_t)v[i].integer;
      for (j=0;j<n;++j) bytes[n-j-1]=(unsigned char)(u>>(8*j));
      if (!mulib_buffer_append(&body,bytes,n)) goto done;
    }
  }
  h=header.length+1;
  for (;;) { k=mulib_sqlite_format_put_varint(h,bytes); if (h==header.length+k) break; h=header.length+k; }
  if (!mulib_buffer_append(&record,bytes,k) || !mulib_buffer_append(&record,header.data,header.length) ||
      !mulib_buffer_append(&record,body.data,body.length) || !mulib_buffer_append(out,record.data,record.length)) goto done;
  rc=0;
done: mulib_buffer_free(&header); mulib_buffer_free(&body); mulib_buffer_free(&record); return rc;
}
int mulib_sqlite_format_open(mulib_sqlite_format_t *out, const void *bytes, size_t n) {
  const unsigned char *p=(const unsigned char *)bytes; mulib_sqlite_format_t f;
  uint32_t pages; size_t i;
  if (!out || (!bytes && n)) return MULIB_SQLITE_MISUSE;
  if (n<100 || memcmp(p,"SQLite format 3",16)) return MULIB_SQLITE_NOTADB;
  memset(&f,0,sizeof f); f.page_size=mulib_sqlite_format_u16_(p+16); if (f.page_size==1) f.page_size=65536;
  if (f.page_size<512 || f.page_size>65536 || (f.page_size&(f.page_size-1)) ||
      n%f.page_size || n/f.page_size>UINT32_MAX-1 || p[20]>f.page_size-480 ||
      p[21]!=64 || p[22]!=32 || p[23]!=32) return MULIB_SQLITE_CORRUPT;
  if (p[19]>2 || !p[19] || !p[18]) return MULIB_SQLITE_UNSUPPORTED;
  for (i=72;i<92;++i) if (p[i]) return MULIB_SQLITE_UNSUPPORTED;
  f.usable_size=f.page_size-p[20]; f.page_count=(uint32_t)(n/f.page_size);
  pages=mulib_endian_load_be32(p+28);
  if (pages && mulib_endian_load_be32(p+24)==mulib_endian_load_be32(p+92)) {
    if (pages>f.page_count) return MULIB_SQLITE_CORRUPT;
    f.page_count=pages;
  }
  f.encoding=mulib_endian_load_be32(p+56); f.schema_format=mulib_endian_load_be32(p+44);
  if (f.encoding>3 || f.schema_format>4) return MULIB_SQLITE_UNSUPPORTED;
  f.data=p; f.length=n; *out=f; return 0;
}
typedef struct mulib_sqlite_format_scan_ {
  const mulib_sqlite_format_t *f; unsigned char *seen; size_t limit;
  mulib_sqlite_format_row_fn callback; void *user; int have_row; int64_t previous;
} mulib_sqlite_format_scan_;
static int mulib_sqlite_format_page_check_(const mulib_sqlite_format_t *f,uint32_t page) {
  const unsigned char *p=f->data+(size_t)(page-1)*f->page_size;
  size_t base=page==1?100:0,header,n,start,i,off,end,k,local,min,freeblock,next,length,j;
  uint64_t size,key; unsigned char *used; int rc=0,type=p[base];
  if (type!=5 && type!=13) return MULIB_SQLITE_UNSUPPORTED;
  n=mulib_sqlite_format_u16_(p+base+3); header=base+(type==5?12:8);
  if (n>(f->usable_size-header)/2 || p[base+7]>60) return MULIB_SQLITE_CORRUPT;
  header+=2*n; start=mulib_sqlite_format_u16_(p+base+5); if (!start) start=65536;
  if (start<header || start>f->usable_size) return MULIB_SQLITE_CORRUPT;
  used=(unsigned char *)calloc(f->usable_size,1); if (!used) return MULIB_SQLITE_NOMEM;
  for (i=0;i<n;++i) {
    off=mulib_sqlite_format_u16_(p+base+(type==5?12:8)+2*i); end=off;
    if (off<start || off>=f->usable_size) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    if (type==5) {
      if (f->usable_size-end<5) { rc=MULIB_SQLITE_CORRUPT; goto done; }
      end+=4; rc=mulib_sqlite_format_varint(p+end,f->usable_size-end,&key,&k); if (rc) goto done; end+=k;
    } else {
      rc=mulib_sqlite_format_varint(p+end,f->usable_size-end,&size,&k); if (rc) goto done; end+=k;
      rc=mulib_sqlite_format_varint(p+end,f->usable_size-end,&key,&k); if (rc) goto done; end+=k;
      if (size>INT32_MAX || size>SIZE_MAX) { rc=MULIB_SQLITE_TOOBIG; goto done; }
      local=(size_t)size;
      if (local>f->usable_size-35) {
        min=((f->usable_size-12)*32/255)-23; local=min+(local-min)%(f->usable_size-4);
        if (local>f->usable_size-35) local=min;
      }
      if (local>f->usable_size-end || (local<size && f->usable_size-end-local<4)) { rc=MULIB_SQLITE_CORRUPT; goto done; }
      end+=local+(local<size?4:0);
    }
    for (j=off;j<end;++j) { if (used[j]) { rc=MULIB_SQLITE_CORRUPT; goto done; } used[j]=1; }
  }
  freeblock=mulib_sqlite_format_u16_(p+base+1);
  while (freeblock) {
    if (freeblock<start || freeblock>f->usable_size-4) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    next=mulib_sqlite_format_u16_(p+freeblock); length=mulib_sqlite_format_u16_(p+freeblock+2);
    if (length<4 || length>f->usable_size-freeblock || (next && next<freeblock+length)) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    for (j=freeblock;j<freeblock+length;++j) { if (used[j]) { rc=MULIB_SQLITE_CORRUPT; goto done; } used[j]=1; }
    freeblock=next;
  }
done: free(used); return rc;
}
static int mulib_sqlite_format_visit_(mulib_sqlite_format_scan_ *s,uint32_t page,size_t depth) {
  const mulib_sqlite_format_t *f=s->f; const unsigned char *p,*q; mulib_buffer_t payload={0};
  size_t base=page==1?100:0,n,i,off,k,pos,local,remaining,take,min,chunk;
  uint64_t size,key; uint32_t next; int type,rc;
  if (!page || page>f->page_count || depth>=64 || s->seen[page]) return MULIB_SQLITE_CORRUPT;
  rc=mulib_sqlite_format_page_check_(f,page); if (rc) return rc;
  s->seen[page]=1; p=f->data+(size_t)(page-1)*f->page_size;
  type=p[base]; if (type!=5 && type!=13) return MULIB_SQLITE_UNSUPPORTED;
  n=mulib_sqlite_format_u16_(p+base+3); pos=base+(type==5?12:8);
  if (n>(f->usable_size-pos)/2) return MULIB_SQLITE_CORRUPT;
  for (i=0;i<n;++i) {
    off=mulib_sqlite_format_u16_(p+pos+2*i);
    if (off<pos+2*n || off>=f->usable_size) return MULIB_SQLITE_CORRUPT;
    if (type==5) {
      if (f->usable_size-off<5) return MULIB_SQLITE_CORRUPT;
      next=mulib_endian_load_be32(p+off);
      rc=mulib_sqlite_format_varint(p+off+4,f->usable_size-off-4,&key,&k); if (rc) return rc;
      rc=mulib_sqlite_format_visit_(s,next,depth+1); if (rc) return rc;
      if (!s->have_row || s->previous>mulib_sqlite_format_signed_(key)) return MULIB_SQLITE_CORRUPT;
      continue;
    }
    rc=mulib_sqlite_format_varint(p+off,f->usable_size-off,&size,&k); if (rc) return rc; off+=k;
    rc=mulib_sqlite_format_varint(p+off,f->usable_size-off,&key,&k); if (rc) return rc; off+=k;
    if (size>s->limit || size>SIZE_MAX) return MULIB_SQLITE_TOOBIG;
    if (s->have_row && s->previous>=mulib_sqlite_format_signed_(key)) return MULIB_SQLITE_CORRUPT;
    local=(size_t)size;
    if (local>f->usable_size-35) {
      min=((f->usable_size-12)*32/255)-23;
      local=min+((size_t)size-min)%(f->usable_size-4);
      if (local>f->usable_size-35) local=min;
    }
    if (local>f->usable_size-off || (local<size && f->usable_size-off-local<4)) return MULIB_SQLITE_CORRUPT;
    if (!mulib_buffer_append(&payload,p+off,local)) return MULIB_SQLITE_NOMEM;
    remaining=(size_t)size-local;
    next=remaining?mulib_endian_load_be32(p+off+local):0;
    while (remaining) {
      if (!next || next>f->page_count || s->seen[next]) { rc=MULIB_SQLITE_CORRUPT; goto done; }
      s->seen[next]=1; q=f->data+(size_t)(next-1)*f->page_size;
      chunk=f->usable_size-4; take=remaining<chunk?remaining:chunk;
      if (!mulib_buffer_append(&payload,q+4,take)) { rc=MULIB_SQLITE_NOMEM; goto done; }
      remaining-=take; next=mulib_endian_load_be32(q);
    }
    if (next) { rc=MULIB_SQLITE_CORRUPT; goto done; }
    rc=s->callback(s->user,mulib_sqlite_format_signed_(key),payload.data,payload.length);
    if (rc) goto done;
    s->have_row=1; s->previous=mulib_sqlite_format_signed_(key);
    mulib_buffer_free(&payload);
  }
  if (type==5) return mulib_sqlite_format_visit_(s,mulib_endian_load_be32(p+base+8),depth+1);
  return 0;
done: mulib_buffer_free(&payload); return rc;
}
int mulib_sqlite_format_scan(const mulib_sqlite_format_t *f,uint32_t root,size_t limit,mulib_sqlite_format_row_fn fn,void *u) {
  mulib_sqlite_format_scan_ s; int rc;
  if (!f || !fn || !f->data) return MULIB_SQLITE_MISUSE;
  memset(&s,0,sizeof s); s.f=f; s.limit=limit?limit:64u*1024u*1024u; s.callback=fn; s.user=u;
  if ((uint64_t)f->page_count+1>SIZE_MAX) return MULIB_SQLITE_TOOBIG;
  s.seen=(unsigned char *)calloc((size_t)f->page_count+1,1); if (!s.seen) return MULIB_SQLITE_NOMEM;
  rc=mulib_sqlite_format_visit_(&s,root,0); free(s.seen); return rc;
}
uint32_t mulib_sqlite_format_page(mulib_sqlite_format_builder_t *b) {
  unsigned char zeros[512]={0}; size_t i,old;
  if (!b || !b->page_size || b->image.length>b->max_bytes || b->page_size>b->max_bytes-b->image.length ||
      b->image.length/b->page_size>=UINT32_MAX-1) return 0;
  old=b->image.length;
  for (i=0;i<b->page_size/sizeof zeros;++i)
    if (!mulib_buffer_append(&b->image,zeros,sizeof zeros)) { b->image.length=old; return 0; }
  return (uint32_t)(b->image.length/b->page_size);
}
int mulib_sqlite_format_builder_init(mulib_sqlite_format_builder_t *b,uint32_t ps,size_t max) {
  unsigned char *p;
  if (!b) return MULIB_SQLITE_MISUSE;
  if (!ps) ps=4096;
  if (ps<512 || ps>65536 || (ps&(ps-1))) return MULIB_SQLITE_MISUSE;
  memset(b,0,sizeof *b); b->page_size=ps; b->max_bytes=max?max:64u*1024u*1024u;
  if (!mulib_sqlite_format_page(b)) { mulib_sqlite_format_builder_free(b); return MULIB_SQLITE_NOMEM; }
  p=b->image.data; memcpy(p,"SQLite format 3",16); mulib_sqlite_format_s16_(p+16,ps==65536?1:ps);
  p[18]=p[19]=1; p[21]=64; p[22]=p[23]=32; mulib_endian_store_be32(p+44,4); mulib_endian_store_be32(p+56,1);
  p[100]=13; mulib_sqlite_format_s16_(p+105,ps); return 0;
}
typedef struct mulib_sqlite_format_child_ { uint32_t page; int64_t high; } mulib_sqlite_format_child_;
static int mulib_sqlite_format_cell_(mulib_sqlite_format_builder_t *b,const mulib_sqlite_format_row_t *r,mulib_buffer_t *cell) {
  unsigned char bytes[9],link[4]; size_t n,local,min,left,take,offset;
  uint32_t page=0,previous=0; const unsigned char *p=(const unsigned char *)r->record;
  n=mulib_sqlite_format_put_varint(r->length,bytes); if (!mulib_buffer_append(cell,bytes,n)) return MULIB_SQLITE_NOMEM;
  n=mulib_sqlite_format_put_varint((uint64_t)r->rowid,bytes); if (!mulib_buffer_append(cell,bytes,n)) return MULIB_SQLITE_NOMEM;
  local=r->length;
  if (local>b->page_size-35) {
    min=((b->page_size-12)*32/255)-23; local=min+(local-min)%(b->page_size-4);
    if (local>b->page_size-35) local=min;
  }
  if (!mulib_buffer_append(cell,p,local)) return MULIB_SQLITE_NOMEM;
  left=r->length-local; offset=local;
  while (left) {
    page=mulib_sqlite_format_page(b); if (!page) return MULIB_SQLITE_NOMEM;
    if (previous) mulib_endian_store_be32(b->image.data+(size_t)(previous-1)*b->page_size,page);
    else { mulib_endian_store_be32(link,page); if (!mulib_buffer_append(cell,link,4)) return MULIB_SQLITE_NOMEM; }
    take=left<b->page_size-4?left:b->page_size-4;
    memcpy(b->image.data+(size_t)(page-1)*b->page_size+4,p+offset,take);
    previous=page; offset+=take; left-=take;
  }
  return 0;
}
static void mulib_sqlite_format_interior_(mulib_sqlite_format_builder_t *b,uint32_t page,
    const mulib_sqlite_format_child_ *children,size_t count) {
  unsigned char *p=b->image.data+(size_t)(page-1)*b->page_size,bytes[9];
  size_t base=page==1?100:0,i,n,end=b->page_size;
  p[base]=5; mulib_sqlite_format_s16_(p+base+3,count-1);
  mulib_endian_store_be32(p+base+8,children[count-1].page);
  for (i=0;i+1<count;++i) {
    n=mulib_sqlite_format_put_varint((uint64_t)children[i].high,bytes); end-=n+4;
    mulib_endian_store_be32(p+end,children[i].page); memcpy(p+end+4,bytes,n);
    mulib_sqlite_format_s16_(p+base+12+2*i,end);
  }
  mulib_sqlite_format_s16_(p+base+5,end);
}
int mulib_sqlite_format_table(mulib_sqlite_format_builder_t *b,uint32_t root,const mulib_sqlite_format_row_t *rows,size_t count) {
  mulib_buffer_t *cells=NULL; mulib_sqlite_format_child_ *children=NULL,*next=NULL;
  unsigned char *p; size_t i,j,bytes=0,base=root==1?100:0,end,n=0,cap,used,take;
  uint32_t page; int rc=0;
  if (!b || !root || root>b->image.length/b->page_size || (!rows && count)) return MULIB_SQLITE_MISUSE;
  if (count>SIZE_MAX/sizeof *cells || count>SIZE_MAX/sizeof *children) return MULIB_SQLITE_TOOBIG;
  cells=(mulib_buffer_t *)calloc(count?count:1,sizeof *cells);
  children=(mulib_sqlite_format_child_ *)calloc(count?count:1,sizeof *children);
  if (!cells || !children) { rc=MULIB_SQLITE_NOMEM; goto done; }
  for (i=0;i<count;++i) {
    if (i && rows[i-1].rowid>=rows[i].rowid) { rc=MULIB_SQLITE_CONSTRAINT; goto done; }
    if (rows[i].length>b->max_bytes || (!rows[i].record && rows[i].length)) { rc=MULIB_SQLITE_TOOBIG; goto done; }
    rc=mulib_sqlite_format_cell_(b,&rows[i],&cells[i]); if (rc) goto done;
    if (cells[i].length>SIZE_MAX-bytes-2) { rc=MULIB_SQLITE_TOOBIG; goto done; }
    bytes+=cells[i].length+2;
  }
  if (bytes<=b->page_size-base-8) {
    p=b->image.data+(size_t)(root-1)*b->page_size; p[base]=13; end=b->page_size;
    mulib_sqlite_format_s16_(p+base+3,count);
    for (i=0;i<count;++i) { end-=cells[i].length; memcpy(p+end,cells[i].data,cells[i].length); mulib_sqlite_format_s16_(p+base+8+2*i,end); }
    mulib_sqlite_format_s16_(p+base+5,end); goto done;
  }
  i=0;
  while (i<count) {
    page=mulib_sqlite_format_page(b); if (!page) { rc=MULIB_SQLITE_NOMEM; goto done; }
    p=b->image.data+(size_t)(page-1)*b->page_size; p[0]=13; end=b->page_size; used=0;
    while (i<count && cells[i].length+2<=end-8-2*used) {
      end-=cells[i].length; memcpy(p+end,cells[i].data,cells[i].length); mulib_sqlite_format_s16_(p+8+2*used,end); ++used; ++i;
    }
    if (!used) { rc=MULIB_SQLITE_TOOBIG; goto done; }
    mulib_sqlite_format_s16_(p+3,used); mulib_sqlite_format_s16_(p+5,end);
    children[n].page=page; children[n++].high=rows[i-1].rowid;
  }
  cap=(b->page_size-12)/15+1;
  while (n>(b->page_size-base-12)/15+1) {
    next=(mulib_sqlite_format_child_ *)calloc(n,sizeof *next); if (!next) { rc=MULIB_SQLITE_NOMEM; goto done; }
    used=0;
    for (i=0;i<n;i+=take) {
      take=n-i<cap?n-i:cap;
      if (n-i-take==1 && take>2) --take;
      page=mulib_sqlite_format_page(b); if (!page) { rc=MULIB_SQLITE_NOMEM; goto done; }
      mulib_sqlite_format_interior_(b,page,children+i,take); next[used].page=page; next[used++].high=children[i+take-1].high;
    }
    free(children); children=next; next=NULL; n=used;
  }
  mulib_sqlite_format_interior_(b,root,children,n);
done:
  if (cells) { for (j=0;j<count;++j) mulib_buffer_free(&cells[j]); free(cells); }
  free(children); free(next); return rc;
}
void mulib_sqlite_format_finish(mulib_sqlite_format_builder_t *b) {
  if (b && b->image.length>=100) {
    mulib_endian_store_be32(b->image.data+28,(uint32_t)(b->image.length/b->page_size));
    mulib_endian_store_be32(b->image.data+92,mulib_endian_load_be32(b->image.data+24));
    /* Independent writer: do not impersonate an upstream SQLite release. */
    mulib_endian_store_be32(b->image.data+96,0);
  }
}
void mulib_sqlite_format_builder_free(mulib_sqlite_format_builder_t *b) { if (b) { mulib_buffer_free(&b->image); memset(b,0,sizeof *b); } }
#ifdef __cplusplus
}
#endif
#endif
