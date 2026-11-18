/** @file mulib-capbroker.h
 * Host-owned exact-match capability dispatch, cumulative quotas, sticky failure,
 * staged transaction hooks. No transport, JSON parser or OS enforcement implied.
 * Define MULIB_IMPL_CAPBROKER and MULIB_IMPL_SANDBOX_PROFILE once.
 */
#ifndef MULIB_CAPBROKER_H
#define MULIB_CAPBROKER_H
#include "mulib-sandbox-profile.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_CAPBROKER_MAX 256u
/** Handler validates typed request payload and authority. Output capacity is
 * bounded; may stage effects but MUST NOT commit. Subject supplied by host. */
typedef int (*mulib_capbroker_handler)(void *ctx,const char *subject,const void *req,size_t req_len,void *reply,size_t cap,size_t *reply_len);
typedef struct mulib_capbroker_op {const char *name,*capability;mulib_capbroker_handler handler;void *context;} mulib_capbroker_op_t;
typedef struct mulib_capbroker_hooks {int (*begin)(void *ctx,const char *subject);int (*commit)(void *ctx);void (*abort)(void *ctx);void (*audit)(void *ctx,const char *subject,const char *operation,int status);void *context;} mulib_capbroker_hooks_t;
/** Borrowed immutable configuration must outlive broker. No reentry or concurrency. */
typedef struct mulib_capbroker {const char *subject;const mulib_capbroker_op_t *ops;size_t op_count;const char *const *grants;size_t grant_count;mulib_capbroker_hooks_t hooks;uint64_t max_bytes,bytes;uint32_t max_requests,requests,max_frame;int active,failed,busy;} mulib_capbroker_t;
/** Intersect untrusted requests with host-approved names, in request order.
 * Output borrows the approved strings; no publication on invalid input/overflow.
 * A host requiring every request must additionally compare counts. */
int mulib_capbroker_intersect(const char *const *requested,size_t n,const char *const *approved,size_t na,const char **effective,size_t capacity,size_t *count);
int mulib_capbroker_init(mulib_capbroker_t *b,const char *subject,const mulib_capbroker_op_t *ops,size_t n,const char *const *grants,size_t ng,const mulib_capbroker_hooks_t *hooks,const mulib_sandbox_profile_t *p);
int mulib_capbroker_begin(mulib_capbroker_t *b);
int mulib_capbroker_call(mulib_capbroker_t *b,const char *operation,const void *req,size_t req_len,void *reply,size_t cap,size_t *reply_len);
/** Success commits once. Failure aborts. Commit must distinguish definite
 * failure from UNCERTAIN; no rollback/retry after uncertain commit. */
int mulib_capbroker_finish(mulib_capbroker_t *b,int worker_success);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CAPBROKER) && !defined(MULIB_CAPBROKER_IMPLEMENTED)
#define MULIB_CAPBROKER_IMPLEMENTED
#include <string.h>
static int mulib_capbroker_name_(const char*s){if(!s||!*s)return 0;size_t n=0;for(;*s;s++,n++)if(n>=127||!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='.'||*s=='_'||*s=='-'))return 0;return 1;}
int mulib_capbroker_intersect(const char*const*r,size_t n,const char*const*a,size_t na,const char**out,size_t cap,size_t*count){
    if((!r&&n)||(!a&&na)||(!out&&cap)||!count||n>MULIB_CAPBROKER_MAX||na>MULIB_CAPBROKER_MAX)return MULIB_SB_INVALID;
    for(size_t i=0;i<n;i++){if(!mulib_capbroker_name_(r[i]))return MULIB_SB_INVALID;for(size_t j=0;j<i;j++)if(!strcmp(r[i],r[j]))return MULIB_SB_INVALID;}
    for(size_t i=0;i<na;i++){if(!mulib_capbroker_name_(a[i]))return MULIB_SB_INVALID;for(size_t j=0;j<i;j++)if(!strcmp(a[i],a[j]))return MULIB_SB_INVALID;}
    const char*tmp[MULIB_CAPBROKER_MAX];size_t used=0;for(size_t i=0;i<n;i++)for(size_t j=0;j<na;j++)if(!strcmp(r[i],a[j])){tmp[used++]=a[j];break;}
    if(used>cap)return MULIB_SB_LIMIT;
    if(used)memcpy(out,tmp,used*sizeof *out);
    *count=used;return 0;}
int mulib_capbroker_init(mulib_capbroker_t*b,const char*subject,const mulib_capbroker_op_t*o,size_t n,const char*const*g,size_t ng,const mulib_capbroker_hooks_t*h,const mulib_sandbox_profile_t*p){if(!b||!mulib_capbroker_name_(subject)||(!o&&n)||n>MULIB_CAPBROKER_MAX||(!g&&ng)||ng>MULIB_CAPBROKER_MAX||mulib_sandbox_profile_validate(p)|| (h&&((!!h->begin+!!h->commit+!!h->abort)!=0&&(!!h->begin+!!h->commit+!!h->abort)!=3)))return MULIB_SB_INVALID;
    for(size_t i=0;i<n;i++){if(!mulib_capbroker_name_(o[i].name)||!mulib_capbroker_name_(o[i].capability)||!o[i].handler)return MULIB_SB_INVALID;for(size_t j=0;j<i;j++)if(!strcmp(o[i].name,o[j].name))return MULIB_SB_INVALID;}
    for(size_t i=0;i<ng;i++){if(!mulib_capbroker_name_(g[i]))return MULIB_SB_INVALID;for(size_t j=0;j<i;j++)if(!strcmp(g[i],g[j]))return MULIB_SB_INVALID;}
    memset(b,0,sizeof *b);b->subject=subject;b->ops=o;b->op_count=n;b->grants=g;b->grant_count=ng;if(h)b->hooks=*h;b->max_bytes=p->max_broker_bytes;b->max_requests=p->max_broker_requests;b->max_frame=p->max_frame_bytes;return 0;}
int mulib_capbroker_begin(mulib_capbroker_t*b){if(!b||!b->subject||b->active||b->busy)return MULIB_SB_STATE;b->bytes=0;b->requests=0;b->failed=0;b->busy=1;int r=b->hooks.begin?b->hooks.begin(b->hooks.context,b->subject):0;b->busy=0;if(r)return r;b->active=1;return 0;}
static int mulib_capbroker_fail_(mulib_capbroker_t*b,const char*name,int r){b->failed=1;if(b->hooks.audit){b->busy=1;b->hooks.audit(b->hooks.context,b->subject,name,r);b->busy=0;}return r;}
int mulib_capbroker_call(mulib_capbroker_t*b,const char*name,const void*req,size_t n,void*reply,size_t cap,size_t*out){if(!b||!b->active||b->busy)return MULIB_SB_STATE;if(b->failed)return MULIB_SB_DENIED;if(!mulib_capbroker_name_(name)||(!req&&n)||(!reply&&cap)||!out)return mulib_capbroker_fail_(b,"invalid",MULIB_SB_INVALID);
    if(b->requests==b->max_requests||n>b->max_frame||n>b->max_bytes-b->bytes)return mulib_capbroker_fail_(b,name,MULIB_SB_LIMIT);
    b->requests++;b->bytes+=n;const mulib_capbroker_op_t*o=NULL;for(size_t i=0;i<b->op_count;i++)if(!strcmp(name,b->ops[i].name)){o=&b->ops[i];break;}
    int granted=0;if(o)for(size_t i=0;i<b->grant_count;i++)if(!strcmp(o->capability,b->grants[i])){granted=1;break;}if(!granted)return mulib_capbroker_fail_(b,name,MULIB_SB_DENIED);
    if(cap>b->max_frame)cap=b->max_frame;
    if((uint64_t)cap>b->max_bytes-b->bytes)cap=(size_t)(b->max_bytes-b->bytes);
    size_t len=0;b->busy=1;int r=o->handler(o->context,b->subject,req,n,reply,cap,&len);b->busy=0;if(r||len>cap)return mulib_capbroker_fail_(b,name,r?r:MULIB_SB_PROTOCOL);b->bytes+=len;*out=len;if(b->hooks.audit){b->busy=1;b->hooks.audit(b->hooks.context,b->subject,name,0);b->busy=0;}return 0;}
int mulib_capbroker_finish(mulib_capbroker_t*b,int ok){if(!b||!b->active||b->busy)return MULIB_SB_STATE;int r=0;b->busy=1;if(!ok||b->failed){if(b->hooks.abort)b->hooks.abort(b->hooks.context);r=MULIB_SB_DENIED;}else if(b->hooks.commit){r=b->hooks.commit(b->hooks.context);if(r&&r!=MULIB_SB_UNCERTAIN)b->hooks.abort(b->hooks.context);}b->active=0;b->failed=0;b->busy=0;return r;}
#endif
