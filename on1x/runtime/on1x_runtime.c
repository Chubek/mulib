#include "on1x_runtime.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#ifndef ON1X_ENV_MALLOC
#define ON1X_ENV_MALLOC malloc
#endif
#ifndef ON1X_ENV_FREE
#define ON1X_ENV_FREE free
#endif
static void environment_error(const char *what){fprintf(stderr,"On1x closure error: %s\n",what);exit(72);}
on1x_env *on1x_env_alloc(size_t size,void (*destroy)(on1x_env *)){
 on1x_env *environment;
 if(size<sizeof(on1x_env))environment_error("invalid environment size");
 environment=(on1x_env *)ON1X_ENV_MALLOC(size);
 if(!environment)environment_error("environment allocation failure");
 environment->references=1;environment->destroy=destroy;environment->next=NULL;
 return environment;
}
void on1x_env_retain(on1x_env *environment){
 if(!environment)return;
 if(!environment->references||environment->references==SIZE_MAX)
  environment_error("environment reference overflow or invalid retain");
 ++environment->references;
}
void on1x_env_release(on1x_env *environment){
 static on1x_env *pending;
 static bool draining;
 if(!environment)return;
 if(!environment->references)environment_error("environment reference underflow");
 if(--environment->references)return;
 environment->next=pending;pending=environment;
 if(draining)return;
 /* Compiler-generated destructors only release captured function values.
  * Queue cascades rather than recursively destroying long capture chains. */
 draining=true;
 while(pending){
  on1x_env *current=pending;pending=current->next;
  if(current->destroy)current->destroy(current);
  ON1X_ENV_FREE(current);
 }
 draining=false;
}
static void arithmetic_error(const char *what){fprintf(stderr,"On1x arithmetic error: %s\n",what);exit(70);}
int64_t on1x_add_i64(int64_t a,int64_t b){
#if defined(__GNUC__) || defined(__clang__)
 int64_t c;if(__builtin_add_overflow(a,b,&c))arithmetic_error("addition overflow");return c;
#else
 if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))arithmetic_error("addition overflow");return a+b;
#endif
}
int64_t on1x_sub_i64(int64_t a,int64_t b){
#if defined(__GNUC__) || defined(__clang__)
 int64_t c;if(__builtin_sub_overflow(a,b,&c))arithmetic_error("subtraction overflow");return c;
#else
 if((b<0&&a>INT64_MAX+b)||(b>0&&a<INT64_MIN+b))arithmetic_error("subtraction overflow");return a-b;
#endif
}
int64_t on1x_mul_i64(int64_t a,int64_t b){
#if defined(__GNUC__) || defined(__clang__)
 int64_t c;if(__builtin_mul_overflow(a,b,&c))arithmetic_error("multiplication overflow");return c;
#else
 if(a==0||b==0)return 0;
 if(a==-1&&b==INT64_MIN)arithmetic_error("multiplication overflow");
 if(b==-1&&a==INT64_MIN)arithmetic_error("multiplication overflow");
 if(a>0){if(b>0){if(a>INT64_MAX/b)arithmetic_error("multiplication overflow");}
 else if(b<INT64_MIN/a)arithmetic_error("multiplication overflow");}
 else if(b>0){if(a<INT64_MIN/b)arithmetic_error("multiplication overflow");}
 else if(a<INT64_MAX/b)arithmetic_error("multiplication overflow");
 return a*b;
#endif
}
int64_t on1x_div_i64(int64_t a,int64_t b){if(!b||(a==INT64_MIN&&b==-1))arithmetic_error("invalid division");return a/b;}
int64_t on1x_mod_i64(int64_t a,int64_t b){if(!b||(a==INT64_MIN&&b==-1))arithmetic_error("invalid modulo");return a%b;}
int64_t on1x_neg_i64(int64_t a){if(a==INT64_MIN)arithmetic_error("negation overflow");return -a;}
void on1x_print_i64(int64_t a){printf("%" PRId64 "\n",a);}
void on1x_print_bool(bool a){puts(a?"true":"false");}
void on1x_print_string(const char *a){puts(a?a:"");}
void on1x_assert(bool a){if(!a){fputs("On1x assertion failed\n",stderr);exit(70);}}

#ifdef ON1X_F64_ABI_VERSION
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) || defined(__NO_SIGNED_ZEROS__) || defined(__ASSOCIATIVE_MATH__) || defined(__RECIPROCAL_MATH__)
#error "On1x f64 forbids fast/finite-only math and relaxed IEEE modes"
#endif
#ifdef __clang__
/* Clang does not expose individual no-honor-NaN/no-signed-zero flags as macros.
 * Restore precise semantics for the primitive implementations explicitly. */
#pragma float_control(precise, on, push)
#endif
#include <xmmintrin.h>
_Static_assert(CHAR_BIT==8 && sizeof(double)==sizeof(uint64_t),"On1x f64 requires binary64 storage");
static void f64_environment(void){
  /* RNE, gradual underflow, all exceptions masked. Do not silently inherit a
   * foreign caller's rounding/FTZ mode or raise a native floating exception. */
  if((_mm_getcsr() & 0xffc0u)!=0x1f80u)
    arithmetic_error("unsupported f64 environment");
}
static uint64_t f64_bits(double a){
  uint64_t bits;memcpy(&bits,&a,sizeof(bits));
  if((bits & UINT64_C(0x7fffffffffffffff))>UINT64_C(0x7ff0000000000000))
    bits=UINT64_C(0x7ff8000000000000);
  return bits;
}
static double f64_value(uint64_t bits){double a;memcpy(&a,&bits,sizeof(a));return a;}
double on1x_f64_from_bits(int64_t bits){return f64_value(f64_bits(f64_value((uint64_t)bits)));}
int64_t on1x_f64_to_bits(double a){
  uint64_t bits=f64_bits(a);
  /* Avoid implementation-defined conversion of an out-of-range unsigned value. */
  return bits<=INT64_MAX?(int64_t)bits:INT64_MIN+(int64_t)(bits-UINT64_C(0x8000000000000000));
}
bool on1x_f64_is_nan(double a){return (f64_bits(a)&UINT64_C(0x7fffffffffffffff))>UINT64_C(0x7ff0000000000000);}
bool on1x_f64_is_finite(double a){return (f64_bits(a)&UINT64_C(0x7ff0000000000000))!=UINT64_C(0x7ff0000000000000);}
bool on1x_f64_signbit(double a){return (f64_bits(a)>>63)!=0;}
double on1x_f64_from_i64(int64_t a){
  volatile int64_t input;volatile double result;
  f64_environment();input=a;result=(double)input;return result;
}
int64_t on1x_f64_to_i64(double a){
  volatile double input;
  f64_environment();input=a;
  if(!on1x_f64_is_finite(a)||input < -0x1p63||input >= 0x1p63)
    arithmetic_error("f64 to i64 out of range");
  return (int64_t)input;
}
/* Volatile binary64 operands/results prevent excess precision, contraction and
 * moving the arithmetic ahead of the environment check, including under LTO. */
#define ON1X_F64_BINARY(name,op) \
double on1x_##name##_f64(double a,double b){ \
  volatile double left,right,result;f64_environment();left=a;right=b; \
  result=left op right;return f64_value(f64_bits(result)); \
}
ON1X_F64_BINARY(add,+)
ON1X_F64_BINARY(sub,-)
ON1X_F64_BINARY(mul,*)
ON1X_F64_BINARY(div,/)
#undef ON1X_F64_BINARY
double on1x_neg_f64(double a){
  f64_environment();return f64_value(f64_bits(f64_value(f64_bits(a)^UINT64_C(0x8000000000000000))));
}
#define ON1X_F64_COMPARE(name,op) \
bool on1x_##name##_f64(double a,double b){ \
  volatile double left,right;f64_environment();left=a;right=b;return left op right; \
}
ON1X_F64_COMPARE(eq,==)
ON1X_F64_COMPARE(ne,!=)
ON1X_F64_COMPARE(lt,<)
ON1X_F64_COMPARE(le,<=)
ON1X_F64_COMPARE(gt,>)
ON1X_F64_COMPARE(ge,>=)
#undef ON1X_F64_COMPARE
void on1x_print_f64(double a){
  uint64_t bits=f64_bits(a),fraction=bits&UINT64_C(0x000fffffffffffff);
  unsigned exponent=(unsigned)((bits>>52)&0x7ffu);
  if(exponent==0x7ffu){puts(fraction?"nan":(bits>>63)?"-inf":"inf");return;}
  printf("%s0x%u.%013" PRIx64 "p%+d\n",(bits>>63)?"-":"",exponent?1u:0u,
    fraction,exponent?(int)exponent-1023:fraction?-1022:0);
}
#ifdef __clang__
#pragma float_control(pop)
#endif
#endif

/* Small C11 data/I/O substrate. Parsing, walking, schema loading/validation and
 * transformations live in the On1x stdlib sources, not in this runtime. */
#ifndef ON1X_DATA_MALLOC
#define ON1X_DATA_MALLOC malloc
#endif
#ifndef ON1X_DATA_FREE
#define ON1X_DATA_FREE free
#endif
#ifndef ON1X_DATA_MAX_TEXT
#define ON1X_DATA_MAX_TEXT (4u*1024u*1024u)
#endif
#ifndef ON1X_DATA_MAX_POOL
#define ON1X_DATA_MAX_POOL (64u*1024u*1024u)
#endif
#ifndef ON1X_DATA_MAX_NODES
#define ON1X_DATA_MAX_NODES 1048576u
#endif
struct on1x_sexpr {on1x_env header;int kind;const char *text;on1x_sexpr *head,*tail;};
struct on1x_buffer {on1x_env header;char *text;size_t length,capacity;};
_Static_assert(offsetof(on1x_sexpr,header)==0,"S-expression reference header must be first");
_Static_assert(offsetof(on1x_buffer,header)==0,"buffer reference header must be first");
struct on1x_saved_text {struct on1x_saved_text *next,*bucket_next;size_t size;char text[];};
static struct on1x_saved_text *on1x_text_pool;
static struct on1x_saved_text *on1x_text_buckets[4096];
static size_t on1x_pool_bytes,on1x_buffer_bytes,on1x_live_data;
static bool on1x_pool_initialized;
static void data_error(const char *message){fprintf(stderr,"On1x tool error: %s\n",message);exit(73);}
void on1x_tool_fail(const char *message){data_error(message);}
static size_t text_size(const char *text){
  size_t n=0;struct on1x_saved_text *saved;
  if(!text)data_error("null text");
  for(saved=on1x_text_buckets[((uintptr_t)text>>3)%4096];saved;saved=saved->bucket_next)if(saved->text==text)return saved->size;
  while(text[n]){if(n==ON1X_DATA_MAX_TEXT)data_error("text byte limit exceeded");++n;}
  return n;
}
static void pool_drop(void){
  while(on1x_text_pool){struct on1x_saved_text *p=on1x_text_pool;on1x_text_pool=p->next;ON1X_DATA_FREE(p);}
  on1x_pool_bytes=0;memset(on1x_text_buckets,0,sizeof(on1x_text_buckets));
}
static char *save_text(size_t size){
  struct on1x_saved_text *saved;size_t amount,bucket;
  if(size>ON1X_DATA_MAX_TEXT)data_error("text byte limit exceeded");
  amount=sizeof(*saved)+size+1;
  if(amount>ON1X_DATA_MAX_POOL-on1x_pool_bytes)data_error("text snapshot pool limit exceeded");
  if(!on1x_pool_initialized){if(atexit(pool_drop))data_error("cannot register text cleanup");on1x_pool_initialized=true;}
  saved=(struct on1x_saved_text *)ON1X_DATA_MALLOC(amount);
  if(!saved)data_error("text allocation failure");
  on1x_pool_bytes+=amount;saved->next=on1x_text_pool;on1x_text_pool=saved;saved->size=size;
  bucket=((uintptr_t)saved->text>>3)%4096;saved->bucket_next=on1x_text_buckets[bucket];on1x_text_buckets[bucket]=saved;
  saved->text[size]='\0';return saved->text;
}
int64_t on1x_text_len(const char *text){return (int64_t)text_size(text);}
int64_t on1x_text_byte(const char *text,int64_t index){
  /* Snapshot lengths are indexed: scanning a wire string is not quadratic. */
  size_t n=text_size(text);
  if(index<0||(uint64_t)index>=n)data_error("text index out of range");
  return (unsigned char)text[(size_t)index];
}
const char *on1x_text_slice(const char *text,int64_t begin,int64_t end){
  size_t n=text_size(text),size;char *out;
  if(begin<0||end<begin||(uint64_t)end>n)data_error("text slice out of range");
  size=(size_t)(end-begin);out=save_text(size);memcpy(out,text+(size_t)begin,size);return out;
}
const char *on1x_text_concat(const char *left,const char *right){
  size_t a=text_size(left),b=text_size(right);char *out;
  if(b>ON1X_DATA_MAX_TEXT-a)data_error("text byte limit exceeded");
  out=save_text(a+b);memcpy(out,left,a);memcpy(out+a,right,b);return out;
}
void on1x_data_retain(void *value){on1x_env_retain((on1x_env *)value);}
void on1x_data_release(void *value){on1x_env_release((on1x_env *)value);}
int64_t on1x_data_live_nodes(void){return (int64_t)on1x_live_data;}
static void data_tick(void){if(on1x_live_data==ON1X_DATA_MAX_NODES)data_error("live data node limit exceeded");++on1x_live_data;}
static void sx_drop(on1x_env *environment){
  on1x_sexpr *node=(on1x_sexpr *)environment;
  on1x_data_release(node->head);on1x_data_release(node->tail);--on1x_live_data;
}
static on1x_sexpr *sx_atom(const char *text,int kind){
  on1x_sexpr *node;size_t n=text_size(text);char *saved=save_text(n);memcpy(saved,text,n);
  data_tick();node=(on1x_sexpr *)on1x_env_alloc(sizeof(*node),sx_drop);
  node->kind=kind;node->text=saved;node->head=node->tail=NULL;return node;
}
on1x_sexpr *on1x_sx_nil(void){return NULL;}
on1x_sexpr *on1x_sx_symbol(const char *text){
  size_t i,n=text_size(text);if(!n)data_error("empty S-expression symbol");
  for(i=0;i<n;++i){unsigned char c=(unsigned char)text[i];if(c<33||c>126||c=='('||c==')'||c=='"')data_error("invalid S-expression symbol");}
  return sx_atom(text,1);
}
on1x_sexpr *on1x_sx_string(const char *text){return sx_atom(text,2);}
on1x_sexpr *on1x_sx_cons(on1x_sexpr *head,on1x_sexpr *tail){
  on1x_sexpr *node;if(tail&&tail->kind!=3)data_error("S-expression tail must be a proper list");
  data_tick();node=(on1x_sexpr *)on1x_env_alloc(sizeof(*node),sx_drop);
  node->kind=3;node->text=NULL;node->head=head;node->tail=tail;on1x_data_retain(head);on1x_data_retain(tail);return node;
}
int64_t on1x_sx_kind(on1x_sexpr *value){return value?value->kind:0;}
on1x_sexpr *on1x_sx_head(on1x_sexpr *value){if(!value||value->kind!=3)data_error("head requires a nonempty list");on1x_data_retain(value->head);return value->head;}
on1x_sexpr *on1x_sx_tail(on1x_sexpr *value){if(!value||value->kind!=3)data_error("tail requires a nonempty list");on1x_data_retain(value->tail);return value->tail;}
const char *on1x_sx_text(on1x_sexpr *value){if(!value||(value->kind!=1&&value->kind!=2))data_error("text requires an S-expression atom");return value->text;}
static void buffer_drop(on1x_env *environment){
  on1x_buffer *buffer=(on1x_buffer *)environment;on1x_buffer_bytes-=buffer->capacity;ON1X_DATA_FREE(buffer->text);--on1x_live_data;
}
on1x_buffer *on1x_buffer_new(void){
  on1x_buffer *buffer;data_tick();buffer=(on1x_buffer *)on1x_env_alloc(sizeof(*buffer),buffer_drop);
  buffer->text=NULL;buffer->length=buffer->capacity=0;return buffer;
}
static void buffer_reserve(on1x_buffer *buffer,size_t amount){
  size_t size,capacity;char *data;
  if(!buffer)data_error("null buffer");
  if(amount>ON1X_DATA_MAX_TEXT-buffer->length)data_error("buffer byte limit exceeded");
  size=buffer->length+amount+1;if(size<=buffer->capacity)return;
  capacity=buffer->capacity?buffer->capacity:64;
  while(capacity<size){if(capacity>(ON1X_DATA_MAX_TEXT+1)/2){capacity=ON1X_DATA_MAX_TEXT+1;break;}capacity*=2;}
  if(capacity-buffer->capacity>ON1X_DATA_MAX_POOL-on1x_buffer_bytes)data_error("live buffer storage limit exceeded");
  data=(char *)ON1X_DATA_MALLOC(capacity);if(!data)data_error("buffer allocation failure");
  if(buffer->length)memcpy(data,buffer->text,buffer->length);
  ON1X_DATA_FREE(buffer->text);on1x_buffer_bytes+=capacity-buffer->capacity;buffer->text=data;buffer->capacity=capacity;data[buffer->length]='\0';
}
void on1x_buffer_append(on1x_buffer *buffer,const char *text){size_t n=text_size(text);buffer_reserve(buffer,n);memcpy(buffer->text+buffer->length,text,n);buffer->length+=n;buffer->text[buffer->length]='\0';}
void on1x_buffer_byte(on1x_buffer *buffer,int64_t byte){if(byte<=0||byte>255)data_error("byte must be in 1..255 (NUL-free text ABI)");buffer_reserve(buffer,1);buffer->text[buffer->length++]=(char)byte;buffer->text[buffer->length]='\0';}
const char *on1x_buffer_text(on1x_buffer *buffer){char *out;if(!buffer)data_error("null buffer");out=save_text(buffer->length);if(buffer->length)memcpy(out,buffer->text,buffer->length);return out;}
int64_t on1x_buffer_len(on1x_buffer *buffer){if(!buffer)data_error("null buffer");return (int64_t)buffer->length;}
void on1x_buffer_clear(on1x_buffer *buffer){if(!buffer)data_error("null buffer");buffer->length=0;if(buffer->text)buffer->text[0]='\0';}
static const char *read_stream(FILE *stream){
  on1x_buffer *buffer=on1x_buffer_new();const char *text;int c;
  while((c=fgetc(stream))!=EOF)on1x_buffer_byte(buffer,c);
  if(ferror(stream))data_error("input read failure");
  text=on1x_buffer_text(buffer);on1x_data_release(buffer);return text;
}
const char *on1x_io_stdin(void){return read_stream(stdin);}
void on1x_io_stdout(const char *text){if(fputs(text,stdout)==EOF||fflush(stdout))data_error("stdout write failure");}
void on1x_io_stderr(const char *text){if(fputs(text,stderr)==EOF||fflush(stderr))data_error("stderr write failure");}
const char *on1x_tool_env(const char *name){const char *value=getenv(name);if(!value)return "";return on1x_text_slice(value,0,on1x_text_len(value));}
const char *on1x_tool_read_file(const char *path){
  FILE *file;const char *text;if(text_size(path)>4096)data_error("file path limit exceeded");
  file=fopen(path,"rb");if(!file)data_error("cannot open input file");text=read_stream(file);if(fclose(file))data_error("input close failure");return text;
}
