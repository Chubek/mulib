/* Internal helpers for staging kernels only; no ABI compatibility claim. */
#ifndef MULIB_BROWSER_ADDITIONS_COMMON_H
#define MULIB_BROWSER_ADDITIONS_COMMON_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
#include <float.h>
#include <time.h>
#define MUBA_OK 1
#define MUBA_NO 0
#define MUBA_ERR (-1)
typedef struct { const unsigned char *data; size_t len; } muba_span;
typedef struct { unsigned char *data; size_t len, cap; } muba_buf;
static inline int muba_span_ok(muba_span s){return s.data || !s.len;}
static inline int muba_eq(muba_span a,muba_span b){return muba_span_ok(a)&&muba_span_ok(b)&&a.len==b.len&&(!a.len||memcmp(a.data,b.data,a.len)==0);}
static inline muba_span muba_str(const char *s){muba_span r={(const unsigned char*)s,s?strlen(s):0};return r;}
static inline int muba_reserve(muba_buf *b,size_t n){if(!b||b->len>b->cap||(!b->data&&b->cap))return 0;if(n<=b->cap)return 1;size_t c=b->cap?b->cap:32;while(c<n){if(c>SIZE_MAX/2){c=n;break;}c*=2;}void *p=realloc(b->data,c);if(!p)return 0;b->data=(unsigned char*)p;b->cap=c;return 1;}
/* Source must not alias b storage; use temporary buffer for self append. */
static inline int muba_append(muba_buf *b,muba_span s){if(!b||!muba_span_ok(s)||s.len>SIZE_MAX-b->len||!muba_reserve(b,b->len+s.len))return 0;if(s.len)memcpy(b->data+b->len,s.data,s.len);b->len+=s.len;return 1;}
static inline int muba_byte(muba_buf *b,unsigned char c){muba_span s={&c,1};return muba_append(b,s);}
static inline void muba_free(muba_buf *b){if(b){free(b->data);memset(b,0,sizeof *b);}}
static inline int muba_commit(muba_buf *out,muba_buf *tmp){if(!out||!tmp||out==tmp)return 0;muba_free(out);*out=*tmp;memset(tmp,0,sizeof *tmp);return 1;}
static inline unsigned char muba_lower(unsigned char c){return c>='A'&&c<='Z'?(unsigned char)(c+32):c;}
static inline int muba_ieq(muba_span a,muba_span b){if(!muba_span_ok(a)||!muba_span_ok(b)||a.len!=b.len)return 0;for(size_t i=0;i<a.len;i++)if(muba_lower(a.data[i])!=muba_lower(b.data[i]))return 0;return 1;}
static inline int muba_space(unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f';}
static inline muba_span muba_trim(muba_span s){if(!muba_span_ok(s))return (muba_span){0,0};while(s.len&&muba_space(s.data[0])){s.data++;s.len--;}while(s.len&&muba_space(s.data[s.len-1]))s.len--;return s;}
static inline int muba_alpha(unsigned char c){c=muba_lower(c);return c>='a'&&c<='z';}
static inline int muba_digit(unsigned char c){return c>='0'&&c<='9';}
static inline int muba_hex(unsigned char c){if(muba_digit(c))return c-'0';c=muba_lower(c);return c>='a'&&c<='f'?c-'a'+10:-1;}
static inline int muba_utf8_put(muba_buf *b,uint32_t c,int surrogate){unsigned char v[4];size_t n;if(c>0x10ffff||(!surrogate&&c>=0xd800&&c<=0xdfff))return 0;if(c<128){v[0]=(unsigned char)c;n=1;}else if(c<2048){v[0]=(unsigned char)(0xc0|(c>>6));v[1]=(unsigned char)(0x80|(c&63));n=2;}else if(c<65536){v[0]=(unsigned char)(0xe0|(c>>12));v[1]=(unsigned char)(0x80|((c>>6)&63));v[2]=(unsigned char)(0x80|(c&63));n=3;}else{v[0]=(unsigned char)(0xf0|(c>>18));v[1]=(unsigned char)(0x80|((c>>12)&63));v[2]=(unsigned char)(0x80|((c>>6)&63));v[3]=(unsigned char)(0x80|(c&63));n=4;}return muba_append(b,(muba_span){v,n});}
static inline int muba_utf8_next(muba_span s,size_t *i,uint32_t *out,int surrogate){if(!i||!out||!muba_span_ok(s)||*i>=s.len)return 0;size_t p=*i;unsigned c=s.data[p++];uint32_t v;unsigned n;if(c<128){v=c;n=0;}else if(c>=0xc2&&c<=0xdf){v=c&31;n=1;}else if(c>=0xe0&&c<=0xef){v=c&15;n=2;}else if(c>=0xf0&&c<=0xf4){v=c&7;n=3;}else return 0;if(n>s.len-p)return 0;for(unsigned j=0;j<n;j++){unsigned q=s.data[p++];if((q&0xc0)!=0x80)return 0;v=(v<<6)|(q&63);}if((n==2&&v<2048)||(n==3&&v<65536)||v>0x10ffff||(!surrogate&&v>=0xd800&&v<=0xdfff))return 0;*out=v;*i=p;return 1;}
static inline int muba_mul_size(size_t a,size_t b,size_t *out){if(!out||(a&&b>SIZE_MAX/a))return 0;*out=a*b;return 1;}
static inline int muba_add_u64(uint64_t a,uint64_t b,uint64_t *out){if(!out||b>UINT64_MAX-a)return 0;*out=a+b;return 1;}
/* Bounded caller-clocked FIFO. Callbacks are borrowed. No threading. */
typedef void (*muba_job_fn)(void *);
typedef struct {muba_job_fn fn;void *ctx;} muba_job;
typedef struct {muba_job *items;size_t head,len,cap;int active;} muba_queue;
static inline int muba_queue_init(muba_queue *q,muba_job *items,size_t cap){if(!q||(!items&&cap))return 0;*q=(muba_queue){items,0,0,cap,0};return 1;}
static inline int muba_queue_push(muba_queue *q,muba_job_fn fn,void *ctx){if(!q||!fn||q->len>=q->cap)return 0;size_t i=(q->head+q->len)%q->cap;q->items[i]=(muba_job){fn,ctx};q->len++;return 1;}
static inline size_t muba_queue_run(muba_queue *q,size_t budget){if(!q||q->active)return 0;q->active=1;size_t n=0;while(q->len&&n<budget){muba_job j=q->items[q->head];q->head=(q->head+1)%q->cap;q->len--;n++;j.fn(j.ctx);}q->active=0;return n;}
/* Byte view over a bounded cursor; never owns memory. */
typedef struct {muba_span s;size_t pos;} muba_cursor;
static inline int muba_take(muba_cursor *c,size_t n,muba_span *out){if(!c||!out||!muba_span_ok(c->s)||c->pos>c->s.len||n>c->s.len-c->pos)return 0;*out=(muba_span){c->s.data?c->s.data+c->pos:NULL,n};c->pos+=n;return 1;}
#endif
