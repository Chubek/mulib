#include "Bridge.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MULL_MULIB_CALLBACK_SLOTS 8
typedef struct mb_callback_slot { mull_mb_callback_fn fn; void *user; } mb_callback_slot;
static pthread_mutex_t mb_callback_mutex = PTHREAD_MUTEX_INITIALIZER;
static int mb_reserve(mb_callback_slot *slots, mull_mb_callback_fn fn, void *user) {
  int result = -1;
  pthread_mutex_lock(&mb_callback_mutex);
  for (int i=0;i<MULL_MULIB_CALLBACK_SLOTS;++i) if (!slots[i].fn) {
    slots[i].fn=fn; slots[i].user=user; result=i; break;
  }
  pthread_mutex_unlock(&mb_callback_mutex); return result;
}
static void mb_dispatch(mb_callback_slot *slot, size_t signature, void *out, void **args) {
  pthread_mutex_lock(&mb_callback_mutex);
  mull_mb_callback_fn fn=slot->fn; void *user=slot->user;
  pthread_mutex_unlock(&mb_callback_mutex);
  if (fn) fn(user,signature,out,args);
}
typedef struct mb_field {
  const char *name; size_t type;
  void *(*address)(void *);
  void (*get)(const void *,void *);
  void (*set)(void *,const void *);
} mb_field;
typedef struct mb_type {
  const char *name; int kind; size_t size,alignment,element,count;
  const mb_field *fields; size_t field_count,result;
  const size_t *arguments; size_t argument_count;
  void (*indirect)(const void *,void *,void **);
  void *(*callback)(mull_mb_callback_fn,void *,void *);
} mb_type;
typedef struct mb_function { const char *name; size_t type; void (*invoke)(void *,void **); } mb_function;
typedef struct mb_constant { size_t type; const void *data; } mb_constant;
typedef struct mb_symbol { const char *name; int kind; size_t id; } mb_symbol;
typedef struct mb_library { const char *name,*header; const mb_symbol *symbols; size_t count; } mb_library;
typedef struct mb_alias { const char *name; size_t type; } mb_alias;

#include "Generated.inc"
#define MB_COUNT(array) (sizeof(array)/sizeof((array)[0]))
static const mb_type *mb_get_type(size_t i) { return i<MB_COUNT(mb_types)?&mb_types[i]:NULL; }
static const mb_field *mb_get_field(size_t t,size_t f) {
  const mb_type *type=mb_get_type(t); return type && f<type->field_count?&type->fields[f]:NULL;
}
/* Case/punctuation folding makes Parsec, parsec, and header names equivalent. */
static void mb_normalize(const char *in,char *out,size_t capacity) {
  size_t n=0;
  for (;*in && n+1<capacity;++in) {
    unsigned char c=(unsigned char)*in;
    if (c>='A' && c<='Z') c=(unsigned char)(c-'A'+'a');
    if ((c>='a' && c<='z') || (c>='0' && c<='9')) out[n++]=(char)c;
  }
  out[n]=0;
}
static int mb_equal(const char *a,const char *b) {
  char x[512],y[512]; if(strlen(a)>=sizeof x || strlen(b)>=sizeof y) return 0;
  mb_normalize(a,x,sizeof x); mb_normalize(b,y,sizeof y); return !strcmp(x,y);
}
static int mb_case_equal(const char *a,const char *b) {
  while(*a && *b) {
    unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
    if(x>='A' && x<='Z') x=(unsigned char)(x-'A'+'a');
    if(y>='A' && y<='Z') y=(unsigned char)(y-'A'+'a');
    if(x!=y) return 0;
  }
  return *a==*b;
}
static int mb_alias_equal(const char *symbol,const char *name,const char *module,int is_type) {
  char full[512],shortname[512],mod[512];
  mb_normalize(symbol,full,sizeof full); mb_normalize(name,shortname,sizeof shortname);
  mb_normalize(module,mod,sizeof mod);
  char *start=full; if(!strncmp(start,"mulib",5)) start+=5;
  if(!strncmp(start,mod,strlen(mod))) start+=strlen(mod);
  if(is_type && *start && start[strlen(start)-1]=='t') start[strlen(start)-1]=0;
  if(!strcmp(start,shortname)) return 1;
  start=full; if(!strncmp(start,"mulib",5)) start+=5;
  return !strcmp(start,shortname);
}
size_t mull_mb_library_count(void) { return MB_COUNT(mb_libraries); }
const char *mull_mb_library_name(size_t i) { return i<MB_COUNT(mb_libraries)?mb_libraries[i].name:NULL; }
const char *mull_mb_library_header(size_t i) { return i<MB_COUNT(mb_libraries)?mb_libraries[i].header:NULL; }
size_t mull_mb_library_find(const char *name) {
  if(!name || !*name || strlen(name)>=512) return MULL_MB_NONE;
  /* Preserve distinct libraries such as container-stub and containerstub. */
  for(size_t i=0;i<MB_COUNT(mb_libraries);++i)
    if(mb_case_equal(name,mb_libraries[i].name) || mb_case_equal(name,mb_libraries[i].header)) return i;
  char stripped[512]; strcpy(stripped,name);
  size_t length=strlen(stripped);
  if(length>2 && !strcmp(stripped+length-2,".h")) stripped[length-2]=0;
  const char *shortname=stripped;
  if(!strncmp(shortname,"include-",8)) shortname+=8;
  if(!strncmp(shortname,"mulib-",6)) shortname+=6;
  size_t found=MULL_MB_NONE;
  for(size_t i=0;i<MB_COUNT(mb_libraries);++i)
    if(mb_equal(shortname,mb_libraries[i].name)) {
      if(found!=MULL_MB_NONE) return MULL_MB_NONE;
      found=i;
    }
  return found;
}
size_t mull_mb_symbol_count(size_t library) { return library<MB_COUNT(mb_libraries)?mb_libraries[library].count:0; }
const char *mull_mb_symbol_name(size_t library,size_t index) {
  return library<MB_COUNT(mb_libraries) && index<mb_libraries[library].count?mb_libraries[library].symbols[index].name:NULL;
}
int mull_mb_symbol_find(size_t library,const char *name,size_t *id) {
  if(library>=MB_COUNT(mb_libraries) || !name || strlen(name)>=512 || !id) return -1;
  const mb_library *lib=&mb_libraries[library];
  for(size_t i=0;i<lib->count;++i) if(!strcmp(name,lib->symbols[i].name)) {
    *id=lib->symbols[i].id; return lib->symbols[i].kind;
  }
  for(int kind=0;kind<2;++kind) for(size_t i=0;i<lib->count;++i) {
    const mb_symbol *s=&lib->symbols[i];
    if(s->kind==kind && mb_alias_equal(s->name,name,lib->name,kind==1)) { *id=s->id; return kind; }
  }
  return -1;
}
size_t mull_mb_type_find(size_t library,const char *name) {
  if(!name || strlen(name)>=512) return MULL_MB_NONE;
  for(size_t i=0;i<MB_COUNT(mb_aliases);++i) if(!strcmp(name,mb_aliases[i].name)) return mb_aliases[i].type;
  for(size_t i=0;i<MB_COUNT(mb_types);++i) if(!strcmp(name,mb_types[i].name)) return i;
  if(library<MB_COUNT(mb_libraries)) {
    const mb_library *lib=&mb_libraries[library];
    for(size_t i=0;i<lib->count;++i) if(lib->symbols[i].kind==1 && mb_alias_equal(lib->symbols[i].name,name,lib->name,1)) return lib->symbols[i].id;
  }
  size_t length=strlen(name);
  while(length && isspace((unsigned char)name[length-1])) --length;
  if(length && name[length-1]=='*') {
    char base[512]; --length;
    while(length && isspace((unsigned char)name[length-1])) --length;
    memcpy(base,name,length); base[length]=0;
    size_t element=mull_mb_type_find(library,base);
    if(element!=MULL_MB_NONE) for(size_t i=0;i<MB_COUNT(mb_types);++i)
      if(mb_types[i].kind==MULL_MB_POINTER && mb_types[i].element==element) return i;
  }
  return MULL_MB_NONE;
}
const char *mull_mb_type_name(size_t i) { const mb_type *t=mb_get_type(i); return t?t->name:NULL; }
int mull_mb_type_kind(size_t i) { const mb_type *t=mb_get_type(i); return t?t->kind:-1; }
size_t mull_mb_type_size(size_t i) { const mb_type *t=mb_get_type(i); return t?t->size:0; }
size_t mull_mb_type_align(size_t i) { const mb_type *t=mb_get_type(i); return t?t->alignment:1; }
size_t mull_mb_type_element(size_t i) { const mb_type *t=mb_get_type(i); return t?t->element:MULL_MB_NONE; }
size_t mull_mb_type_count(size_t i) { const mb_type *t=mb_get_type(i); return t?t->count:0; }
int mull_mb_type_pointee_const(size_t i) { return mb_pointee_const(i); }
size_t mull_mb_field_count(size_t i) { const mb_type *t=mb_get_type(i); return t?t->field_count:0; }
const char *mull_mb_field_name(size_t t,size_t f) { const mb_field *v=mb_get_field(t,f); return v?v->name:NULL; }
size_t mull_mb_field_type(size_t t,size_t f) { const mb_field *v=mb_get_field(t,f); return v?v->type:MULL_MB_NONE; }
int mull_mb_field_readonly(size_t t,size_t f) { const mb_field *v=mb_get_field(t,f); return !v || !v->set; }
void *mull_mb_field_address(size_t t,size_t f,void *object) { const mb_field *v=mb_get_field(t,f); return v && object?v->address(object):NULL; }
int mull_mb_field_get(size_t t,size_t f,const void *object,void *out) { const mb_field *v=mb_get_field(t,f); if(!v || !object || !out) return 0; v->get(object,out); return 1; }
int mull_mb_field_set(size_t t,size_t f,void *object,const void *in) { const mb_field *v=mb_get_field(t,f); if(!v || !v->set || !object || !in) return 0; v->set(object,in); return 1; }
int mull_mb_number_read(size_t t,const void *v,double *out) { return v && out && mb_numeric_read(t,v,out); }
int mull_mb_number_write(size_t t,void *v,double n) { return v && mb_numeric_write(t,v,n); }
int mull_mb_integer_read(size_t t,const void *v,char *out,size_t n) { return v && out && n && mb_numeric_integer_read(t,v,out,n); }
int mull_mb_integer_write(size_t t,void *v,const char *s) { return v && s && mb_numeric_integer_write(t,v,s); }
size_t mull_mb_function_type(size_t f) { return f<MB_COUNT(mb_functions)?mb_functions[f].type:MULL_MB_NONE; }
size_t mull_mb_result_type(size_t s) { const mb_type *t=mb_get_type(s); return t?t->result:MULL_MB_NONE; }
size_t mull_mb_argument_count(size_t s) { const mb_type *t=mb_get_type(s); return t?t->argument_count:0; }
size_t mull_mb_argument_type(size_t s,size_t i) { const mb_type *t=mb_get_type(s); return t && i<t->argument_count?t->arguments[i]:MULL_MB_NONE; }
void mull_mb_invoke(size_t f,void *out,void **args) { if(f<MB_COUNT(mb_functions)) mb_functions[f].invoke(out,args); }
void mull_mb_invoke_pointer(size_t s,const void *fn,void *out,void **args) { const mb_type *t=mb_get_type(s); if(t && t->indirect) t->indirect(fn,out,args); }
size_t mull_mb_constant_type(size_t i) { return i<MB_COUNT(mb_constants)?mb_constants[i].type:MULL_MB_NONE; }
const void *mull_mb_constant_data(size_t i) { return i<MB_COUNT(mb_constants)?mb_constants[i].data:NULL; }
void *mull_mb_allocate(size_t type,size_t count) {
  const mb_type *t=mb_get_type(type); if(!t || !t->size || !count || count>SIZE_MAX/t->size) return NULL;
  size_t size=t->size*count,alignment=t->alignment;
  if(alignment<sizeof(void*)) alignment=sizeof(void*);
  void *out=NULL; if(posix_memalign(&out,alignment,size)) return NULL;
  memset(out,0,size); return out;
}
void mull_mb_deallocate(void *p) { free(p); }
void mull_mb_pointer_write(void *out,void *p) { memcpy(out,&p,sizeof p); }
void *mull_mb_pointer_read(const void *in) { void *p=NULL; memcpy(&p,in,sizeof p); return p; }
void *mull_mb_callback_create(size_t s,mull_mb_callback_fn fn,void *user,void *out) {
  const mb_type *t=mb_get_type(s); return t && t->callback && fn && out?t->callback(fn,user,out):NULL;
}
void mull_mb_callback_release(void *token) {
  if(!token) return;
  pthread_mutex_lock(&mb_callback_mutex);
  mb_callback_slot *slot=(mb_callback_slot*)token; slot->fn=NULL; slot->user=NULL;
  pthread_mutex_unlock(&mb_callback_mutex);
}
typedef struct mb_allocation { void *memory,*callback; struct mb_allocation *next; } mb_allocation;
struct mull_mb_arena { size_t bytes,limit; mb_allocation *head; };
mull_mb_arena *mull_mb_arena_create(size_t limit) {
  mull_mb_arena *a=(mull_mb_arena*)calloc(1,sizeof(*a)); if(a) a->limit=limit; return a;
}
void mull_mb_arena_destroy(mull_mb_arena *a) {
  if(!a) return;
  while(a->head) {
    mb_allocation *n=a->head; a->head=n->next;
    mull_mb_callback_release(n->callback); free(n->memory); free(n);
  }
  free(a);
}
void *mull_mb_arena_allocate(mull_mb_arena *a,size_t type,size_t count) {
  size_t size=mull_mb_type_size(type);
  if(!a || !size || !count || count>SIZE_MAX/size) return NULL;
  size*=count;
  if(sizeof(mb_allocation)>a->limit-a->bytes || size>a->limit-a->bytes-sizeof(mb_allocation)) return NULL;
  mb_allocation *n=(mb_allocation*)calloc(1,sizeof(*n)); if(!n) return NULL;
  n->memory=mull_mb_allocate(type,count); if(!n->memory) { free(n); return NULL; }
  n->next=a->head; a->head=n; a->bytes+=size+sizeof(*n); return n->memory;
}
int mull_mb_arena_callback(mull_mb_arena *a,size_t s,mull_mb_callback_fn fn,void *user,void *out) {
  if(!a || sizeof(mb_allocation)>a->limit-a->bytes) return 0;
  mb_allocation *n=(mb_allocation*)calloc(1,sizeof(*n)); if(!n) return 0;
  n->callback=mull_mb_callback_create(s,fn,user,out);
  if(!n->callback) { free(n); return 0; }
  n->next=a->head; a->head=n; a->bytes+=sizeof(*n); return 1;
}
