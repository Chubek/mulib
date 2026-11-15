#define _POSIX_C_SOURCE 200809L
#include "basis_native.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef BASIS_MALLOC
#define BASIS_MALLOC malloc
#endif

#define BASIS_MAX_ARRAYS 1024
#define BASIS_MAX_ARRAY_ELEMENTS 262144
#define BASIS_MAX_ARRAY_BYTES (16u*1024u*1024u)
#define BASIS_MAX_FILE_BYTES (2u*1024u*1024u)
#ifndef BASIS_MAX_SNAPSHOT_BYTES
#define BASIS_MAX_SNAPSHOT_BYTES (32u*1024u*1024u)
#endif
#define BASIS_MAX_SNAPSHOT_TEXT (4u*1024u*1024u)
struct basis_slot { int64_t *data; size_t length; uint64_t generation; };
static struct basis_slot slots[BASIS_MAX_ARRAYS];
static size_t allocated_array_bytes;
static void cleanup(void);
static int cleanup_registered;
/* Single-threaded registry. Handles carry a generation to reject use-after-free.
   Release handles explicitly; no C compiler can prove client ownership here. */
static void fail(const char *message) {
  fprintf(stderr,"On1x Basis native adapter: %s\n",message);
  exit(73);
}
static struct basis_slot *lookup(int64_t handle) {
  unsigned long long raw=(unsigned long long)handle;
  unsigned long long index=(raw&65535ULL);
  unsigned long long generation=(raw>>16);
  struct basis_slot *slot;
  if(handle<=0 || index==0 || index>BASIS_MAX_ARRAYS || generation==0)
    fail("invalid array handle");
  slot=&slots[index-1];
  if(slot->generation!=generation || !slot->data)
    fail("stale array handle or double release");
  return slot;
}
int64_t basis_native_arr_new(int64_t length,int64_t initial) {
  size_t n,bytes,i;
  struct basis_slot *slot=NULL;
  if(length<0 || length>BASIS_MAX_ARRAY_ELEMENTS)fail("array length outside supported range");
  n=(size_t)length;
  bytes=n*sizeof(int64_t);
  if(bytes>BASIS_MAX_ARRAY_BYTES-allocated_array_bytes)fail("array memory budget exceeded");
  for(i=0;i<BASIS_MAX_ARRAYS;++i) {
    if(!slots[i].data && slots[i].generation<((uint64_t)INT64_MAX>>16)-1) {
      slot=&slots[i];break;
    }
  }
  if(!slot)fail("array handle capacity exhausted");
  if(!cleanup_registered){if(atexit(cleanup))fail("atexit registration failed");cleanup_registered=1;}
  slot->data=(int64_t *)BASIS_MALLOC(bytes?bytes:sizeof(int64_t));
  if(!slot->data)fail("array allocation failed");
  for(i=0;i<n;++i)slot->data[i]=initial;
  slot->length=n;
  ++slot->generation;
  allocated_array_bytes+=bytes;
  return (int64_t)((slot->generation<<16)|((size_t)(slot-slots)+1));
}
int64_t basis_native_arr_len(int64_t handle) { return (int64_t)lookup(handle)->length; }
int64_t basis_native_arr_get(int64_t handle,int64_t index) {
  const struct basis_slot *s=lookup(handle);
  if(index<0 || (uint64_t)index>=s->length)fail("array subscript out of bounds");
  return s->data[index];
}
void basis_native_arr_set(int64_t handle,int64_t index,int64_t value) {
  struct basis_slot *s=lookup(handle);
  if(index<0 || (uint64_t)index>=s->length)fail("array update out of bounds");
  s->data[index]=value;
}
int64_t basis_native_arr_copy(int64_t handle) {
  const struct basis_slot *s=lookup(handle);
  int64_t result=basis_native_arr_new((int64_t)s->length,0);
  struct basis_slot *dst=lookup(result);
  if(s->length)memcpy(dst->data,s->data,s->length*sizeof(int64_t));
  return result;
}
void basis_native_arr_free(int64_t handle) {
  struct basis_slot *s=lookup(handle);
  allocated_array_bytes-=s->length*sizeof(int64_t);
  free(s->data);s->data=NULL;s->length=0;
}

struct basis_snapshot { struct basis_snapshot *next; size_t size; char content[]; };
static struct basis_snapshot *snapshots;
static size_t snapshot_bytes;
static void cleanup(void) {
  while(snapshots){struct basis_snapshot *next=snapshots->next;
    free(snapshots);snapshots=next;}
  for(size_t i=0;i<BASIS_MAX_ARRAYS;++i)free(slots[i].data);
}
/* Release a caller's temporary before reporting an allocation/budget failure. */
static char *snapshot_with_temporary(size_t n,void *temporary) {
  struct basis_snapshot *s;
  if(n>BASIS_MAX_SNAPSHOT_TEXT || n>SIZE_MAX-sizeof(*s)-1) {
    free(temporary);fail("snapshot size exceeded");
  }
  size_t cost=sizeof(*s)+n+1;
  if(cost>BASIS_MAX_SNAPSHOT_BYTES-snapshot_bytes) {
    free(temporary);fail("snapshot budget exceeded");
  }
  if(!cleanup_registered){if(atexit(cleanup)){free(temporary);fail("atexit registration failed");}cleanup_registered=1;}
  s=(struct basis_snapshot *)BASIS_MALLOC(cost);
  if(!s){free(temporary);fail("snapshot allocation failed");}
  s->size=n;s->content[n]='\0';s->next=snapshots;snapshots=s;
  snapshot_bytes+=cost;
  return s->content;
}
static char *snapshot(size_t n){return snapshot_with_temporary(n,NULL);}
static bool write_raw(const char *path,const char *bytes,size_t size,const char *mode) {
  FILE *f;
  if(!path || !bytes || strlen(path)>4096 || size>BASIS_MAX_FILE_BYTES)return false;
  f=fopen(path,mode);if(!f)return false;
  bool ok=fwrite(bytes,1,size,f)==size;
  if(fclose(f)!=0)ok=false;
  return ok;
}
bool basis_native_write_text(const char *path,const char *content) {
  if(!content)return false;
  return write_raw(path,content,strlen(content),"wb");
}
bool basis_native_append_text(const char *path,const char *content) {
  if(!content)return false;
  return write_raw(path,content,strlen(content),"ab");
}
static int hex_digit(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
const char *basis_native_read_hex(const char *path) {
  char *out;
  unsigned char chunk[4096];
  size_t total=0,count;
  static const char alphabet[]="0123456789abcdef";
  if(!path || strlen(path)>4096)fail("invalid binary file path");
  FILE *f=fopen(path,"rb");
  if(!f)fail("binary read: cannot open file");
  /* Read into bounded temporary binary buffer, preserving every byte incl NUL. */
  unsigned char *raw=(unsigned char *)BASIS_MALLOC(BASIS_MAX_FILE_BYTES+1);
  if(!raw){fclose(f);fail("binary input allocation failed");}
  while((count=fread(chunk,1,sizeof(chunk),f))>0) {
    if(count>BASIS_MAX_FILE_BYTES-total){free(raw);fclose(f);fail("binary input too large");}
    memcpy(raw+total,chunk,count);total+=count;
  }
  if(ferror(f)){free(raw);fclose(f);fail("binary input failed");}
  if(fclose(f)){free(raw);fail("binary input close failed");}
  out=snapshot_with_temporary(total*2,raw);
  for(size_t i=0;i<total;++i) {
    out[2*i]=alphabet[raw[i]>>4];out[2*i+1]=alphabet[raw[i]&15];
  }
  free(raw);return out;
}
bool basis_native_write_hex(const char *path,const char *hex_text) {
  size_t n;
  unsigned char *raw;
  bool ok;
  if(!hex_text)return false;
  n=strlen(hex_text);
  if(n%2 || n/2>BASIS_MAX_FILE_BYTES)return false;
  raw=(unsigned char *)BASIS_MALLOC(n/2?n/2:1);
  if(!raw)return false;
  for(size_t i=0;i<n/2;++i) {
    int hi=hex_digit(hex_text[2*i]),lo=hex_digit(hex_text[2*i+1]);
    if(hi<0||lo<0){free(raw);return false;}
    raw[i]=(unsigned char)(hi*16+lo);
  }
  ok=write_raw(path,(const char *)raw,n/2,"wb");
  free(raw);return ok;
}
bool basis_native_fs_exists(const char *path) {struct stat s;return path && stat(path,&s)==0;}
bool basis_native_fs_isdir(const char *path) {
  struct stat s;return path && stat(path,&s)==0 && S_ISDIR(s.st_mode);
}
int64_t basis_native_fs_size(const char *path) {
  struct stat s;if(!path||stat(path,&s))return -1;
  if(s.st_size<0)return -1;
  return (int64_t)s.st_size;
}
bool basis_native_fs_remove(const char *path){return path&&remove(path)==0;}
bool basis_native_fs_mkdir(const char *path){return path&&mkdir(path,0777)==0;}
bool basis_native_fs_rename(const char *a,const char *b){return a&&b&&rename(a,b)==0;}
bool basis_native_fs_chdir(const char *path){return path&&chdir(path)==0;}
const char *basis_native_fs_cwd(void){
  char stack[4096];
  if(!getcwd(stack,sizeof(stack)))fail("cannot obtain working directory");
  size_t n=strlen(stack);char *result=snapshot(n);memcpy(result,stack,n);return result;
}
void basis_native_process_exit(int64_t code){
  if(code<0 || code>255)fail("process exit code must be 0..255");
  exit((int)code);
}
int64_t basis_native_time_unix_seconds(void){
  time_t now=time(NULL);
  if(now==(time_t)-1)fail("time failed");
  return (int64_t)now;
}
int64_t basis_native_time_monotonic_ns(void){
  struct timespec t;
  #ifdef CLOCK_MONOTONIC
  if(clock_gettime(CLOCK_MONOTONIC,&t))fail("monotonic clock failed");
  #else
  if(timespec_get(&t,TIME_UTC)!=TIME_UTC)fail("clock failed");
  #endif
  if(t.tv_sec<0 || t.tv_nsec<0 || t.tv_nsec>=1000000000L ||
     (uint64_t)t.tv_sec>((uint64_t)INT64_MAX-(uint64_t)t.tv_nsec)/1000000000ULL)
    fail("clock outside i64 nanosecond range");
  return (int64_t)t.tv_sec*1000000000LL+(int64_t)t.tv_nsec;
}
const char *basis_native_date_utc(int64_t unix_seconds){
  time_t t=(time_t)unix_seconds;
  struct tm utc;
  char buf[32];
  if((int64_t)t!=unix_seconds)fail("date cannot be represented by host time_t");
  if(!gmtime_r(&t,&utc))fail("UTC conversion failed");
  size_t n=strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%SZ",&utc);
  if(!n)fail("UTC formatting failed");
  char *out=snapshot(n);memcpy(out,buf,n);return out;
}
