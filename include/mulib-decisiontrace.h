/** @file mulib-decisiontrace.h
 * @brief Caller-owned, bounded, structured decision audit events.
 * Define MULIB_IMPL_DECISIONTRACE once. No allocations or I/O. Hash is not
 * cryptographic; persist and authenticate records in the application layer.
 * The caller owns the buffer, keeps it live for all operations and must not
 * mutate it concurrently with kernel calls.
 */
#ifndef MULIB_DECISIONTRACE_H
#define MULIB_DECISIONTRACE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_DECISIONTRACE_CODE_MAX 31

typedef enum mulib_decisiontrace_kind {
    MULIB_DECISIONTRACE_OBSERVATION=1,
    MULIB_DECISIONTRACE_TRANSFORM=2,
    MULIB_DECISIONTRACE_RULE=3,
    MULIB_DECISIONTRACE_GUARDRAIL=4,
    MULIB_DECISIONTRACE_FINAL=5
} mulib_decisiontrace_kind_t;
typedef struct mulib_decisiontrace_event {
    mulib_decisiontrace_kind_t kind;
    uint64_t input_fingerprint64; /* provenance hint, NONCRYPTOGRAPHIC */
    int64_t at;                  /* Unix seconds */
    int64_t before_minor, after_minor;
    char code[32];              /* NUL-terminated stable event code */
} mulib_decisiontrace_event_t;
typedef struct mulib_decisiontrace {
    mulib_decisiontrace_event_t *events; /* borrowed caller capacity */
    size_t count, capacity;
} mulib_decisiontrace_t;
int mulib_decisiontrace_init(mulib_decisiontrace_t *trace,
                             mulib_decisiontrace_event_t *buffer,size_t capacity);
/** Appends only if valid and capacity sufficient; no partial append. */
int mulib_decisiontrace_append(mulib_decisiontrace_t *trace,
                               const mulib_decisiontrace_event_t *event);
/** Stable FNV-1a 64 over only occupied fields (not padding); returns 0 on error
 * and preserves *out. Not suitable for authentication. */
int mulib_decisiontrace_fingerprint64(const mulib_decisiontrace_t *trace,uint64_t *out);
/** Encode canonical trace v1: ASCII `FPDT`, byte 1, then a little-endian u64
 * event count. Each event encodes kind, input_fingerprint64, timestamp, before,
 * after and code length as six little-endian u64 words, then code bytes
 * without NUL. Signed values are converted modulo 2^64. No struct padding.
 * Call with dst=NULL and capacity=0 to query the exact size. A non-NULL dst
 * must not overlap trace storage or the size output. Returns 1 on success;
 * on invalid input or insufficient space, both outputs are unchanged.
 * To make a durable proof, the host must separately bind exact input content,
 * units and policy identity and authenticate the resulting record. */
int mulib_decisiontrace_encode_v1(const mulib_decisiontrace_t *trace,
                                 void *dst,size_t capacity,size_t *out_size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DECISIONTRACE) && !defined(MULIB_DECISIONTRACE_IMPLEMENTED)
#define MULIB_DECISIONTRACE_IMPLEMENTED
/* FINPRED_AGENT_TODO: Bind exact input content digests, contract and policy IDs, selection diagnostics, decisions and execution attempts in a reviewed authenticated record; add durable storage outside the kernel.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static size_t mulib_decisiontrace_code_len_(const char *s){
    size_t i;for(i=0;i<32 && s[i];++i){
        unsigned char c=(unsigned char)s[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return 0;
    }
    return i>0 && i<32 && s[i]==0?i:0;
}
static int mulib_decisiontrace_valid_capacity_(size_t cap){
    return cap>0&&cap<=SIZE_MAX/sizeof(mulib_decisiontrace_event_t);
}
static size_t mulib_decisiontrace_event_code_len_(const mulib_decisiontrace_event_t *e){
    if(e->kind<MULIB_DECISIONTRACE_OBSERVATION||e->kind>MULIB_DECISIONTRACE_FINAL||
       e->at<0)return 0;
    return mulib_decisiontrace_code_len_(e->code);
}
int mulib_decisiontrace_init(mulib_decisiontrace_t *t,
                             mulib_decisiontrace_event_t *b,size_t cap){
    if(!t||!b||!mulib_decisiontrace_valid_capacity_(cap))return 0;
    t->events=b;t->count=0;t->capacity=cap;return 1;
}
int mulib_decisiontrace_append(mulib_decisiontrace_t *t,
                               const mulib_decisiontrace_event_t *e){
    mulib_decisiontrace_event_t clean;
    size_t n;
    if(!t||!t->events||!e||!mulib_decisiontrace_valid_capacity_(t->capacity)||
       t->count>=t->capacity||!(n=mulib_decisiontrace_event_code_len_(e)))return 0;
    memset(&clean,0,sizeof(clean));clean.kind=e->kind;
    clean.input_fingerprint64=e->input_fingerprint64;clean.at=e->at;
    clean.before_minor=e->before_minor;clean.after_minor=e->after_minor;
    memcpy(clean.code,e->code,n);
    t->events[t->count++]=clean;return 1;
}
static uint64_t mulib_decisiontrace_hash_u64_(uint64_t h,uint64_t n){
    unsigned j;for(j=0;j<8;++j){h^=(n>>(8*j))&255U;h*=UINT64_C(1099511628211);}return h;
}
int mulib_decisiontrace_fingerprint64(const mulib_decisiontrace_t *t,uint64_t *out){
    size_t i,j,n;uint64_t h=UINT64_C(14695981039346656037);
    if(!t||!t->events||!out||!mulib_decisiontrace_valid_capacity_(t->capacity)||
       t->count>t->capacity||t->count>UINT64_MAX)return 0;
    h=mulib_decisiontrace_hash_u64_(h,t->count);
    for(i=0;i<t->count;++i){const mulib_decisiontrace_event_t *e=&t->events[i];
        if(!(n=mulib_decisiontrace_event_code_len_(e)))return 0;
        h=mulib_decisiontrace_hash_u64_(h,(uint64_t)e->kind);
        h=mulib_decisiontrace_hash_u64_(h,e->input_fingerprint64);
        h=mulib_decisiontrace_hash_u64_(h,(uint64_t)e->at);
        h=mulib_decisiontrace_hash_u64_(h,(uint64_t)e->before_minor);
        h=mulib_decisiontrace_hash_u64_(h,(uint64_t)e->after_minor);
        h=mulib_decisiontrace_hash_u64_(h,n);
        for(j=0;j<n;++j){h^=(unsigned char)e->code[j];h*=UINT64_C(1099511628211);}
    }
    *out=h;return 1;
}
static unsigned char *mulib_decisiontrace_write_u64_(unsigned char *dst,uint64_t v){
    unsigned i;
    for(i=0;i<8;++i)dst[i]=(unsigned char)(v>>(8*i));
    return dst+8;
}
int mulib_decisiontrace_encode_v1(const mulib_decisiontrace_t *t,
                                 void *dst,size_t cap,size_t *out_size){
    static const unsigned char prefix[]={0x46,0x50,0x44,0x54,1};
    size_t i,total=sizeof(prefix)+8;
    unsigned char *p=(unsigned char *)dst;
    if(!t||!t->events||!out_size||(!dst&&cap)||
       !mulib_decisiontrace_valid_capacity_(t->capacity)||
       t->count>t->capacity||t->count>UINT64_MAX)return 0;
    for(i=0;i<t->count;++i){
        size_t n=mulib_decisiontrace_event_code_len_(&t->events[i]);
        if(!n||total>SIZE_MAX-48-n)return 0;
        total+=48+n;
    }
    if(p){
        if(cap<total)return 0;
        memcpy(p,prefix,sizeof(prefix));p+=sizeof(prefix);
        p=mulib_decisiontrace_write_u64_(p,t->count);
        for(i=0;i<t->count;++i){
            const mulib_decisiontrace_event_t *e=&t->events[i];
            size_t n=strlen(e->code); /* fully validated above */
            p=mulib_decisiontrace_write_u64_(p,(uint64_t)e->kind);
            p=mulib_decisiontrace_write_u64_(p,e->input_fingerprint64);
            p=mulib_decisiontrace_write_u64_(p,(uint64_t)e->at);
            p=mulib_decisiontrace_write_u64_(p,(uint64_t)e->before_minor);
            p=mulib_decisiontrace_write_u64_(p,(uint64_t)e->after_minor);
            p=mulib_decisiontrace_write_u64_(p,n);
            memcpy(p,e->code,n);p+=n;
        }
    }
    *out_size=total;return 1;
}
#ifdef __cplusplus
}
#endif
#endif
