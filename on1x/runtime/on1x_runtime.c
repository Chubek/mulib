#include "on1x_runtime.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <errno.h>
#include <unistd.h>
#endif
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
#ifndef ON1X_DEFER_MAX_RECORDS
#define ON1X_DEFER_MAX_RECORDS 4096
#endif
#ifndef ON1X_DEFER_MAX_BYTES
#define ON1X_DEFER_MAX_BYTES (8 * 1024 * 1024)
#endif
struct on1x_defer_record {on1x_defer_record *next;void (*invoke)(on1x_env *);on1x_env *environment;size_t bytes;};
static void defer_error(const char *what,const char *file,size_t line,size_t column){
 fprintf(stderr,"%s:%zu:%zu: On1x defer error: %s\n",file,line,column,what);exit(72);
}
on1x_env *on1x_defer_register(on1x_defer_stack *stack,size_t size,
    void (*invoke)(on1x_env *),void (*destroy)(on1x_env *),
    const char *file,size_t line,size_t column){
 on1x_defer_record *record;on1x_env *environment=NULL;size_t bytes;
 if(!invoke||(size&&size<sizeof(on1x_env)))defer_error("invalid registration",file,line,column);
 if(stack->count>=ON1X_DEFER_MAX_RECORDS)defer_error("registration limit exceeded",file,line,column);
 if(size>SIZE_MAX-sizeof(*record))defer_error("capture byte limit exceeded",file,line,column);
 bytes=sizeof(*record)+size;
 if(bytes>ON1X_DEFER_MAX_BYTES||stack->bytes>ON1X_DEFER_MAX_BYTES-bytes)
  defer_error("capture byte limit exceeded",file,line,column);
 record=(on1x_defer_record *)ON1X_ENV_MALLOC(sizeof(*record));
 if(!record)defer_error("record allocation failure",file,line,column);
 if(size){
  environment=(on1x_env *)ON1X_ENV_MALLOC(size);
  if(!environment){ON1X_ENV_FREE(record);defer_error("capture allocation failure",file,line,column);}
  environment->references=1;environment->destroy=destroy;environment->next=NULL;
 }
 record->invoke=invoke;record->environment=environment;record->bytes=bytes;
 record->next=stack->head;stack->head=record;++stack->count;stack->bytes+=bytes;
 return environment;
}
void on1x_defer_run(on1x_defer_stack *stack){
 while(stack->head){on1x_defer_record *record=stack->head;stack->head=record->next;--stack->count;
  stack->bytes-=record->bytes;
  record->invoke(record->environment);on1x_env_release(record->environment);ON1X_ENV_FREE(record);
 }
}
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
static uint64_t unsigned_range(uint64_t a,uint64_t maximum){
  if(a>maximum)arithmetic_error("unsigned conversion out of range");
  return a;
}
_Static_assert(CHAR_BIT==8 && sizeof(uint8_t)==1 && sizeof(uint16_t)==2 && sizeof(uint32_t)==4 && sizeof(uint64_t)==8,
  "On1x unsigned ABI requires exact 8/16/32/64-bit integers and 8-bit bytes");
static int64_t unsigned_i64(uint64_t a){
  if(a>INT64_MAX)arithmetic_error("unsigned to i64 out of range");
  return (int64_t)a;
}
static int64_t unsigned_bits(uint64_t a){
  return a<=INT64_MAX?(int64_t)a:INT64_MIN+(int64_t)(a-UINT64_C(0x8000000000000000));
}
static unsigned unsigned_shift(int64_t n,unsigned width){
  if(n<0||(uint64_t)n>=width)arithmetic_error("unsigned shift count out of range");
  return (unsigned)n;
}
/* Widen before arithmetic: C integer promotions must never turn u16 multiply
 * or shifts into overflowing signed int operations. u64 wrapping is defined. */
#define ON1X_UNSIGNED_IMPL(N,T,MAXIMUM,WIDTH) \
 T on1x_add_##N(T a,T b){if((uint64_t)a>(uint64_t)(MAXIMUM)-(uint64_t)b)arithmetic_error("unsigned addition overflow");return (T)((uint64_t)a+(uint64_t)b);} \
 T on1x_sub_##N(T a,T b){if(a<b)arithmetic_error("unsigned subtraction underflow");return (T)((uint64_t)a-(uint64_t)b);} \
 T on1x_mul_##N(T a,T b){if(b&&(uint64_t)a>(uint64_t)(MAXIMUM)/(uint64_t)b)arithmetic_error("unsigned multiplication overflow");return (T)((uint64_t)a*(uint64_t)b);} \
 T on1x_div_##N(T a,T b){if(!b)arithmetic_error("unsigned division by zero");return (T)(a/b);} \
 T on1x_mod_##N(T a,T b){if(!b)arithmetic_error("unsigned modulo by zero");return (T)(a%b);} \
 void on1x_print_##N(T a){printf("%" PRIu64 "\n",(uint64_t)a);} \
 T on1x_##N##_from_i64(int64_t a){if(a<0)arithmetic_error("negative unsigned conversion");return (T)unsigned_range((uint64_t)a,MAXIMUM);} \
 int64_t on1x_##N##_to_i64(T a){return unsigned_i64((uint64_t)a);} \
 T on1x_##N##_from_bits(int64_t a){return (T)(uint64_t)a;} \
 int64_t on1x_##N##_to_bits(T a){return unsigned_bits((uint64_t)a);} \
 T on1x_##N##_wrapping_add(T a,T b){return (T)((uint64_t)a+(uint64_t)b);} \
 T on1x_##N##_wrapping_sub(T a,T b){return (T)((uint64_t)a-(uint64_t)b);} \
 T on1x_##N##_wrapping_mul(T a,T b){return (T)((uint64_t)a*(uint64_t)b);} \
 T on1x_##N##_and(T a,T b){return (T)((uint64_t)a&(uint64_t)b);} \
 T on1x_##N##_or(T a,T b){return (T)((uint64_t)a|(uint64_t)b);} \
 T on1x_##N##_xor(T a,T b){return (T)((uint64_t)a^(uint64_t)b);} \
 T on1x_##N##_not(T a){return (T)~(uint64_t)a;} \
 T on1x_##N##_shl(T a,int64_t n){unsigned s=unsigned_shift(n,WIDTH);if((uint64_t)a>((uint64_t)(MAXIMUM)>>s))arithmetic_error("unsigned left shift overflow");return (T)((uint64_t)a<<s);} \
 T on1x_##N##_shr(T a,int64_t n){return (T)((uint64_t)a>>unsigned_shift(n,WIDTH));} \
 T on1x_##N##_from_u8(uint8_t a){return (T)unsigned_range(a,MAXIMUM);} \
 T on1x_##N##_from_u16(uint16_t a){return (T)unsigned_range(a,MAXIMUM);} \
 T on1x_##N##_from_u32(uint32_t a){return (T)unsigned_range(a,MAXIMUM);} \
 T on1x_##N##_from_u64(uint64_t a){return (T)unsigned_range(a,MAXIMUM);}
ON1X_UNSIGNED_IMPL(u8,uint8_t,UINT8_MAX,8)
ON1X_UNSIGNED_IMPL(u16,uint16_t,UINT16_MAX,16)
ON1X_UNSIGNED_IMPL(u32,uint32_t,UINT32_MAX,32)
ON1X_UNSIGNED_IMPL(u64,uint64_t,UINT64_MAX,64)
#undef ON1X_UNSIGNED_IMPL
void on1x_assert(bool a){if(!a){fputs("On1x assertion failed\n",stderr);exit(70);}}
static void value_error(const char *message,const char *file,size_t line){
  fprintf(stderr,"%s:%zu: On1x value error: %s\n",file,line,message);exit(70);
}
void on1x_bounds_index(int64_t index,int64_t length,const char *file,size_t line){
  if(length<0||index<0||index>=length)value_error("index out of bounds",file,line);
}
void on1x_bounds_slice(int64_t lo,int64_t hi,int64_t length,const char *file,size_t line){
  if(length<0||lo<0||hi<lo||hi>length)value_error("slice bounds out of range",file,line);
}
void on1x_invalid_tag(const char *file,size_t line){value_error("invalid sum discriminant",file,line);}

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
#ifdef ON1X_F32_ABI_VERSION
_Static_assert(sizeof(float)==sizeof(uint32_t),"On1x f32 requires binary32 storage");
static uint32_t f32_bits(float a){
  uint32_t bits;memcpy(&bits,&a,sizeof(bits));
  if((bits&UINT32_C(0x7fffffff))>UINT32_C(0x7f800000))bits=UINT32_C(0x7fc00000);
  return bits;
}
static float f32_value(uint32_t bits){float a;memcpy(&a,&bits,sizeof(a));return a;}
float on1x_f32_from_bits(int64_t bits){
  if(bits<0||(uint64_t)bits>UINT32_MAX)arithmetic_error("f32 bit pattern out of range");
  return f32_value(f32_bits(f32_value((uint32_t)bits)));
}
int64_t on1x_f32_to_bits(float a){return (int64_t)f32_bits(a);}
bool on1x_f32_is_nan(float a){return (f32_bits(a)&UINT32_C(0x7fffffff))>UINT32_C(0x7f800000);}
bool on1x_f32_is_finite(float a){return (f32_bits(a)&UINT32_C(0x7f800000))!=UINT32_C(0x7f800000);}
bool on1x_f32_signbit(float a){return (f32_bits(a)>>31)!=0;}
float on1x_f32_from_i64(int64_t a){
  volatile int64_t input;volatile float result;f64_environment();input=a;result=(float)input;return result;
}
int64_t on1x_f32_to_i64(float a){
  volatile float input;f64_environment();input=a;
  if(!on1x_f32_is_finite(a)||(double)input < -0x1p63||(double)input >= 0x1p63)
    arithmetic_error("f32 to i64 out of range");
  return (int64_t)input;
}
float on1x_f32_from_f64(double a){
  uint64_t bits=f64_bits(a),magnitude=bits&UINT64_C(0x7fffffffffffffff);
  uint32_t sign=(uint32_t)(bits>>32)&UINT32_C(0x80000000);
  volatile double input;volatile float result;f64_environment();
  if(magnitude>UINT64_C(0x7ff0000000000000))return f32_value(UINT32_C(0x7fc00000));
  if(magnitude==UINT64_C(0x7ff0000000000000))return f32_value(sign|UINT32_C(0x7f800000));
  /* C does not define an out-of-range double->float cast. Handle the RNE
   * overflow interval before casting, including the FLT_MAX/Inf midpoint. */
  if(a>(double)FLT_MAX||a<-(double)FLT_MAX){
    double absolute=a<0?-a:a;
    return f32_value(sign|(absolute<0x1.ffffffp127?UINT32_C(0x7f7fffff):UINT32_C(0x7f800000)));
  }
  input=a;result=(float)input;return f32_value(f32_bits(result));
}
double on1x_f64_from_f32(float a){
  volatile float input;volatile double result;f64_environment();input=f32_value(f32_bits(a));result=(double)input;
  return f64_value(f64_bits(result));
}
#define ON1X_F32_BINARY(name,op) \
float on1x_##name##_f32(float a,float b){ \
  volatile float left,right,result;f64_environment();left=a;right=b; \
  result=left op right;return f32_value(f32_bits(result)); \
}
ON1X_F32_BINARY(add,+)
ON1X_F32_BINARY(sub,-)
ON1X_F32_BINARY(mul,*)
ON1X_F32_BINARY(div,/)
#undef ON1X_F32_BINARY
float on1x_neg_f32(float a){f64_environment();return f32_value(f32_bits(f32_value(f32_bits(a)^UINT32_C(0x80000000))));}
#define ON1X_F32_COMPARE(name,op) \
bool on1x_##name##_f32(float a,float b){volatile float left,right;f64_environment();left=a;right=b;return left op right;}
ON1X_F32_COMPARE(eq,==)
ON1X_F32_COMPARE(ne,!=)
ON1X_F32_COMPARE(lt,<)
ON1X_F32_COMPARE(le,<=)
ON1X_F32_COMPARE(gt,>)
ON1X_F32_COMPARE(ge,>=)
#undef ON1X_F32_COMPARE
void on1x_print_f32(float a){
  uint32_t bits=f32_bits(a),fraction=bits&UINT32_C(0x007fffff);
  unsigned exponent=(unsigned)((bits>>23)&0xffu);
  if(exponent==0xffu){puts(fraction?"nan":(bits>>31)?"-inf":"inf");return;}
  printf("%s0x%u.%06" PRIx32 "p%+d\n",(bits>>31)?"-":"",exponent?1u:0u,
    fraction<<1,exponent?(int)exponent-127:fraction?-126:0);
}
static uint64_t unsigned_float(double a,double bound,bool finite){
  if(!finite||a<0||a>=bound)arithmetic_error("float to unsigned out of range");
  return (uint64_t)a;
}
#define ON1X_UFLOAT_IMPL(N,T,BOUND) \
 T on1x_##N##_from_f32(float a){f64_environment();return (T)unsigned_float((double)a,BOUND,on1x_f32_is_finite(a));} \
 T on1x_##N##_from_f64(double a){f64_environment();return (T)unsigned_float(a,BOUND,on1x_f64_is_finite(a));} \
 float on1x_f32_from_##N(T a){volatile T input;volatile float result;f64_environment();input=a;result=(float)input;return result;} \
 double on1x_f64_from_##N(T a){volatile T input;volatile double result;f64_environment();input=a;result=(double)input;return result;}
ON1X_UFLOAT_IMPL(u8,uint8_t,0x1p8)
ON1X_UFLOAT_IMPL(u16,uint16_t,0x1p16)
ON1X_UFLOAT_IMPL(u32,uint32_t,0x1p32)
ON1X_UFLOAT_IMPL(u64,uint64_t,0x1p64)
#undef ON1X_UFLOAT_IMPL
#endif
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
bool on1x_diagnostics_sideband(const char *wire){
 const char *descriptor=getenv("ON1X_PPX_DIAGNOSTICS_FD");size_t length,written=0;
 if(!wire||!descriptor||strcmp(descriptor,"3")!=0||strcmp(on1x_tool_env("ON1X_PPX_PROTOCOL"),"1")!=0)return false;
 length=strlen(wire);if(!length||length>32768||wire[length-1]!='\n')return false;
#if defined(__unix__) || defined(__APPLE__)
 while(written<length){
  ssize_t count=write(3,wire+written,length-written);
  if(count<0&&errno==EINTR)continue;
  if(count<=0)return false;
  written+=(size_t)count;
 }
 return true;
#else
 (void)written;return false;
#endif
}
const char *on1x_tool_env(const char *name){const char *value=getenv(name);if(!value)return "";return on1x_text_slice(value,0,on1x_text_len(value));}
const char *on1x_tool_read_file(const char *path){
  FILE *file;const char *text;if(text_size(path)>4096)data_error("file path limit exceeded");
  file=fopen(path,"rb");if(!file)data_error("cannot open input file");text=read_stream(file);if(fclose(file))data_error("input close failure");return text;
}
