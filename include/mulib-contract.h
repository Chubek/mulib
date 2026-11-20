/** @file mulib-contract.h
 * @brief Borrowed, versioned FinPRED data-contract envelopes with bounded
 * metadata validation and a deterministic NONCRYPTOGRAPHIC fingerprint.
 * C11; define MULIB_IMPL_CONTRACT and link canonical MULIB_IMPL_SHA once.
 * No allocations, filesystem access or hidden Lua execution.
 */
#ifndef MULIB_CONTRACT_H
#define MULIB_CONTRACT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/** An envelope for in-memory bytes, not a dataset locator or object handle.
 * Identifier strings must be readable through their NUL terminator (or
 * through byte 127 when no earlier terminator exists). data points to at least
 * data_size readable bytes, or may be NULL if size is zero. The caller keeps
 * all borrowed storage alive and immutable through each call; no pointer is
 * retained or included in a fingerprint or digest. */
typedef struct mulib_contract {
    const char *id;             /* stable contract identity, ASCII identifier */
    const char *kind;           /* semantic type, e.g. hospitality.offer */
    const char *schema_id;      /* schema identifier, NOT a schema implementation */
    const char *source_id;      /* provenance identity */
    uint32_t major, minor;      /* major must be nonzero */
    int64_t observed_at;        /* Unix seconds, >=0 */
    int64_t valid_from;         /* Unix seconds, inclusive */
    int64_t valid_until;        /* Unix seconds, exclusive */
    uint32_t quality_flags;     /* opaque to this kernel */
    const void *data;          /* borrowed bytes; may include NUL */
    size_t data_size;
} mulib_contract_t;

/** Every string must be nonempty, <=127 bytes, ASCII [A-Za-z0-9._:-].
 * valid_until>valid_from and observed_at>=0. Payload must not exceed max_bytes.
 * Output does not change because validation is read-only. */
int mulib_contract_valid(const mulib_contract_t *c, size_t max_bytes);
/** At time t, contract may be used iff observed_at<=t and valid interval covers t. */
int mulib_contract_usable_at(const mulib_contract_t *c, int64_t t, size_t max_bytes);
/** Exact kind/schema match and equal major; minor may differ (application must
 * independently verify schema evolution compatibility). */
int mulib_contract_same_family(const mulib_contract_t *a, const mulib_contract_t *b);
/** Stable FNV-1a-64 over length-prefixed canonical fields and payload.
 * NOT collision resistant; never use as authentication, signing or trust proof.
 * Return 1 on success, 0 on invalid input; preserve *out on failure. */
int mulib_contract_fingerprint64(const mulib_contract_t *c, size_t max_bytes,
                                  uint64_t *out);
/** SHA-256 content identity, not source authentication. Canonical byte format:
 * ASCII "mulib.contract.v1" without NUL, then id, kind, schema_id, source_id
 * (each as u64 byte length followed by raw bytes), then major, minor,
 * observed_at, valid_from, valid_until, quality_flags and payload byte count
 * (all u64), then raw payload. All u64 fields are little endian.
 * On failure preserves all output bytes. out_capacity must be at least 32;
 * only the first 32 bytes are written on success. */
int mulib_contract_digest_sha256(const mulib_contract_t *c,size_t max_bytes,
    unsigned char *out,size_t out_capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTRACT) && !defined(MULIB_CONTRACT_IMPLEMENTED)
#define MULIB_CONTRACT_IMPLEMENTED
/* FINPRED_AGENT_TODO: Define validated JSON/YAML schema-evolution rules, source trust, dataset locator and immutable snapshot semantics, and timestamp provenance requirements. Keep digest distinct from noncryptographic fingerprint64.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include "mulib-sha.h"
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static size_t mulib_contract_strlen_(const char *s) {
    size_t i;
    if (!s) return 0;
    for (i=0; i<=127 && s[i]; ++i) {
        unsigned char c=(unsigned char)s[i];
        if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
              (c>='0'&&c<='9')||c=='.'||c=='_'||c==':'||c=='-')) return 0;
    }
    return (i>0 && i<=127 && s[i]==0) ? i : 0;
}
int mulib_contract_valid(const mulib_contract_t *c, size_t limit) {
    if (!c || !mulib_contract_strlen_(c->id) || !mulib_contract_strlen_(c->kind) ||
        !mulib_contract_strlen_(c->schema_id) || !mulib_contract_strlen_(c->source_id) ||
        c->major==0 || c->observed_at<0 || c->valid_from<0 ||
        c->valid_until<=c->valid_from || c->data_size>limit ||
        (c->data_size && !c->data)) return 0;
    return 1;
}
int mulib_contract_usable_at(const mulib_contract_t *c, int64_t t, size_t limit) {
    return t>=0 && mulib_contract_valid(c,limit) && c->observed_at<=t &&
           c->valid_from<=t && t<c->valid_until;
}
int mulib_contract_same_family(const mulib_contract_t *a, const mulib_contract_t *b) {
    return a && b && mulib_contract_valid(a,SIZE_MAX) &&
           mulib_contract_valid(b,SIZE_MAX) && a->major==b->major &&
           strcmp(a->kind,b->kind)==0 && strcmp(a->schema_id,b->schema_id)==0;
}
static uint64_t mulib_contract_bytehash_(uint64_t h,const void *v,size_t n){
    const unsigned char *p=(const unsigned char *)v;size_t i;
    for(i=0;i<n;++i) { h^=(uint64_t)p[i];h*=UINT64_C(1099511628211); }
    return h;
}
static uint64_t mulib_contract_u64hash_(uint64_t h,uint64_t v){
    unsigned i;for(i=0;i<8;++i){unsigned char b=(unsigned char)(v>>(8*i));
        h=mulib_contract_bytehash_(h,&b,1);}
    return h;
}
static uint64_t mulib_contract_stringhash_(uint64_t h,const char *s){
    size_t n=strlen(s); h=mulib_contract_u64hash_(h,(uint64_t)n);
    return mulib_contract_bytehash_(h,s,n);
}
int mulib_contract_fingerprint64(const mulib_contract_t *c, size_t lim,uint64_t *out){
    uint64_t h=UINT64_C(14695981039346656037);
    if(!out||!mulib_contract_valid(c,lim)||c->data_size>UINT64_MAX)return 0;
    h=mulib_contract_stringhash_(h,c->id);
    h=mulib_contract_stringhash_(h,c->kind);
    h=mulib_contract_stringhash_(h,c->schema_id);
    h=mulib_contract_stringhash_(h,c->source_id);
    h=mulib_contract_u64hash_(h,c->major);
    h=mulib_contract_u64hash_(h,c->minor);
    h=mulib_contract_u64hash_(h,(uint64_t)c->observed_at);
    h=mulib_contract_u64hash_(h,(uint64_t)c->valid_from);
    h=mulib_contract_u64hash_(h,(uint64_t)c->valid_until);
    h=mulib_contract_u64hash_(h,c->quality_flags);
    h=mulib_contract_u64hash_(h,(uint64_t)c->data_size);
    h=mulib_contract_bytehash_(h,c->data,c->data_size);
    *out=h;return 1;
}
static int mulib_contract_digest_u64_(mulib_sha_ctx *ctx,uint64_t n){
    unsigned char b[8];
    unsigned i;
    for(i=0;i<8;++i)b[i]=(unsigned char)(n>>(8*i));
    return mulib_sha_update(ctx,b,sizeof(b));
}
int mulib_contract_digest_sha256(const mulib_contract_t *c,size_t lim,
    unsigned char *out,size_t cap){
    static const char domain[]="mulib.contract.v1";
    mulib_sha_ctx ctx;
    unsigned char digest[32];
    const char *ids[4];
    size_t i,n;
    if(!out||cap<sizeof(digest)||!mulib_contract_valid(c,lim)||
       c->data_size>UINT64_MAX)return 0;
    ids[0]=c->id;ids[1]=c->kind;ids[2]=c->schema_id;ids[3]=c->source_id;
    if(!mulib_sha_init(&ctx,MULIB_SHA256)||
       !mulib_sha_update(&ctx,domain,sizeof(domain)-1))return 0;
    for(i=0;i<4;++i){
        n=strlen(ids[i]);
        if(!mulib_contract_digest_u64_(&ctx,n)||!mulib_sha_update(&ctx,ids[i],n))return 0;
    }
    if(!mulib_contract_digest_u64_(&ctx,c->major)||
       !mulib_contract_digest_u64_(&ctx,c->minor)||
       !mulib_contract_digest_u64_(&ctx,(uint64_t)c->observed_at)||
       !mulib_contract_digest_u64_(&ctx,(uint64_t)c->valid_from)||
       !mulib_contract_digest_u64_(&ctx,(uint64_t)c->valid_until)||
       !mulib_contract_digest_u64_(&ctx,c->quality_flags)||
       !mulib_contract_digest_u64_(&ctx,(uint64_t)c->data_size)||
       !mulib_sha_update(&ctx,c->data,c->data_size)||
       !mulib_sha_final(&ctx,digest,sizeof(digest)))return 0;
    memcpy(out,digest,sizeof(digest));
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif
