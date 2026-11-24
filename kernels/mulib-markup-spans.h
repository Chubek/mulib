/** @file mulib-markup-spans.h
 * @brief Owned text with overlapping, stand-off half-open byte-range annotations.
 * ISO C11; define MULIB_IMPL_CDD_MARKUP_SPANS in one implementation TU.
 * MULIB_IMPL_MARKUP_SPANS remains an alias.
 * No Unicode grapheme semantics: offsets are bytes, and callers must supply
 * UTF-8 codepoint boundaries if their application requires them.
 */
#ifndef MULIB_MARKUP_SPANS_H
#define MULIB_MARKUP_SPANS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_mkir_span {
 size_t begin,end;char *label;uint32_t flags;
} mulib_mkir_span_t;
typedef struct mulib_mkir_spans {
 char *text;size_t length,capacity;
 mulib_mkir_span_t *items;size_t count,item_capacity;
} mulib_mkir_spans_t;
typedef struct mulib_mkir_run {
 size_t begin,end;size_t *annotations,count;
} mulib_mkir_run_t;
typedef struct mulib_mkir_runs {
 mulib_mkir_run_t *items;size_t count;
} mulib_mkir_runs_t;
/* The input is copied; init needs an empty/zeroed object. */
int mulib_mkir_spans_init(mulib_mkir_spans_t *s,const void *text,size_t length);
/* Appending text preserves annotation coordinates; call on initialized object.
   A range inside text may include the current terminator but not spare capacity. */
int mulib_mkir_spans_append(mulib_mkir_spans_t *s,const void *text,size_t length);
int mulib_mkir_spans_add(mulib_mkir_spans_t *s,size_t begin,size_t end,
                         const char *label,uint32_t flags,size_t *id);
int mulib_mkir_spans_validate(const mulib_mkir_spans_t *s);
/* Partition at nonempty annotation boundaries, including uncovered text.
   Each run owns its active annotations in insertion (ID) order.
   Zero-length spans are retained but do not affect the partition.
   out must be zero-initialized and stays unchanged on failure. */
int mulib_mkir_spans_runs(const mulib_mkir_spans_t *s,mulib_mkir_runs_t *out);
/* As above, with inclusive limits on runs and the sum of their active counts.
   Zero is a literal limit, not "unlimited". Input inspection and scratch space
   for sorting boundaries are not limited by these output budgets. */
int mulib_cdd_mkir_spans_runs_limited(const mulib_mkir_spans_t *s,
                                    size_t max_runs,size_t max_memberships,
                                    mulib_mkir_runs_t *out);
void mulib_mkir_runs_free(mulib_mkir_runs_t *r);
void mulib_mkir_spans_free(mulib_mkir_spans_t *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CDD_MARKUP_SPANS) || defined(MULIB_IMPL_MARKUP_SPANS)
#ifndef MULIB_MARKUP_SPANS_IMPLEMENTED
#define MULIB_MARKUP_SPANS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static void *mulib_mkir_spans_grow_(void *data,size_t *cap,size_t need,size_t unit){
 size_t max=SIZE_MAX/unit,n=*cap;void *p;
 if(need>max)return NULL;
 if(need<=n)return data;
 if(!n)n=max<16?need:16;
 while(n<need){if(n>max/2){n=need;break;}n*=2;}
 p=realloc(data,n*unit);if(!p)return NULL;
 *cap=n;return p;
}
static int mulib_mkir_spans_shape_(const mulib_mkir_spans_t *s){
 return s&&s->text&&s->length<s->capacity&&s->count<=s->item_capacity&&
   s->item_capacity<=SIZE_MAX/sizeof(*s->items)&&
   (s->items!=NULL)==(s->item_capacity!=0)&&s->text[s->length]==0;
}
int mulib_mkir_spans_init(mulib_mkir_spans_t *s,const void *text,size_t length){
 if(!s||s->text||s->items||s->length||s->capacity||s->count||s->item_capacity||
    (!text&&length)||length==SIZE_MAX)return 0;
 s->text=(char*)malloc(length+1);if(!s->text)return 0;
 if(length)memcpy(s->text,text,length);
 s->text[length]=0;s->length=length;s->capacity=length+1;return 1;
}
int mulib_mkir_spans_append(mulib_mkir_spans_t *s,const void *text,size_t n){
 size_t at=0;int aliased=0;char *p;
 if(!mulib_mkir_spans_shape_(s)||(!text&&n)||n>=SIZE_MAX-s->length)return 0;
 if(!n)return 1;
 /* Detect self append safely across realloc. */
 {
   uintptr_t a=(uintptr_t)text,b=(uintptr_t)s->text;
   if(a>=b&&a-b<s->capacity){
     at=(size_t)(a-b);
     if(at>s->length||n>s->length+1-at)return 0;
     aliased=1;
   }
 }
 p=(char*)mulib_mkir_spans_grow_(s->text,&s->capacity,s->length+n+1,1);
 if(!p)return 0;
 s->text=p;
 memmove(s->text+s->length,aliased?s->text+at:text,n);
 s->length+=n;s->text[s->length]=0;return 1;
}
int mulib_mkir_spans_add(mulib_mkir_spans_t *s,size_t begin,size_t end,const char *label,uint32_t flags,size_t *id){
 mulib_mkir_span_t *p;char *copy;size_t n,i;
 if(!mulib_mkir_spans_shape_(s)||!label||!*label||begin>end||end>s->length||
    s->count==SIZE_MAX/sizeof(*s->items))return 0;
 n=strlen(label);if(n==SIZE_MAX)return 0;
 copy=(char*)malloc(n+1);if(!copy)return 0;memcpy(copy,label,n+1);
 p=(mulib_mkir_span_t*)mulib_mkir_spans_grow_(s->items,&s->item_capacity,s->count+1,sizeof(*s->items));
 if(!p){free(copy);return 0;}
 s->items=p;i=s->count++;
 {mulib_mkir_span_t item={begin,end,copy,flags};p[i]=item;}
 if(id)*id=i;
 return 1;
}
int mulib_mkir_spans_validate(const mulib_mkir_spans_t *s){
 if(!mulib_mkir_spans_shape_(s))return 0;
 for(size_t i=0;i<s->count;i++)if(!s->items[i].label||!*s->items[i].label||
    s->items[i].begin>s->items[i].end||s->items[i].end>s->length)return 0;
 return 1;
}
static int mulib_mkir_size_cmp_(const void *a,const void *b){
 size_t x=*(const size_t*)a,y=*(const size_t*)b;return x<y?-1:x>y?1:0;
}
/* Every queried offset occurs among the cuts. */
static size_t mulib_mkir_spans_cut_(const size_t *cuts,size_t n,size_t value){
 size_t lo=0,hi=n;
 while(lo<hi){
   size_t mid=lo+(hi-lo)/2;
   if(cuts[mid]<value)lo=mid+1;else hi=mid;
 }
 return lo;
}
int mulib_cdd_mkir_spans_runs_limited(const mulib_mkir_spans_t *s,size_t max_runs,
                                     size_t max_memberships,mulib_mkir_runs_t *out){
 size_t *cuts=NULL,k=0,n=0,nonempty=0,total=0;
 mulib_mkir_runs_t tmp={NULL,0};
 if(!out||out->items||out->count||!mulib_mkir_spans_validate(s))return 0;
 if(!s->length)return 1;
 if(!max_runs)return 0;
 for(size_t i=0;i<s->count;i++)if(s->items[i].begin<s->items[i].end)nonempty++;
 if(nonempty>(SIZE_MAX/sizeof(*cuts)-2)/2)return 0;
 cuts=(size_t*)malloc((2*nonempty+2)*sizeof(*cuts));if(!cuts)return 0;
 cuts[k++]=0;cuts[k++]=s->length;
 for(size_t i=0;i<s->count;i++)if(s->items[i].begin<s->items[i].end){
   cuts[k++]=s->items[i].begin;cuts[k++]=s->items[i].end;
 }
 qsort(cuts,k,sizeof(*cuts),mulib_mkir_size_cmp_);
 for(size_t i=0;i<k;i++)if(!n||cuts[n-1]!=cuts[i])cuts[n++]=cuts[i];
 if(n-1>max_runs||n-1>SIZE_MAX/sizeof(*tmp.items))goto fail;
 tmp.items=(mulib_mkir_run_t*)malloc((n-1)*sizeof(*tmp.items));
 if(!tmp.items)goto fail;
 tmp.count=n-1;
 for(size_t i=0;i<tmp.count;i++){
   mulib_mkir_run_t run={cuts[i],cuts[i+1],NULL,0};tmp.items[i]=run;
 }
 /* Size each run without scanning all spans for every cut. */
 for(size_t i=0;i<s->count;i++){
   const mulib_mkir_span_t *a=&s->items[i];size_t first,past;
   if(a->begin==a->end)continue;
   first=mulib_mkir_spans_cut_(cuts,n,a->begin);
   past=mulib_mkir_spans_cut_(cuts,n,a->end);
   if(past-first>max_memberships-total)goto fail;
   total+=past-first;
   for(size_t j=first;j<past;j++)tmp.items[j].count++;
 }
 for(size_t i=0;i<tmp.count;i++){
   mulib_mkir_run_t *r=&tmp.items[i];
   if(r->count){
     /* Per-run count is at most s->count, already bounded by item width. */
     r->annotations=(size_t*)malloc(r->count*sizeof(*r->annotations));
     if(!r->annotations)goto fail;
   }
   r->count=0;
 }
 for(size_t i=0;i<s->count;i++){
   const mulib_mkir_span_t *a=&s->items[i];size_t first,past;
   if(a->begin==a->end)continue;
   first=mulib_mkir_spans_cut_(cuts,n,a->begin);
   past=mulib_mkir_spans_cut_(cuts,n,a->end);
   for(size_t j=first;j<past;j++){
     mulib_mkir_run_t *r=&tmp.items[j];r->annotations[r->count++]=i;
   }
 }
 *out=tmp;free(cuts);return 1;
 fail:free(cuts);mulib_mkir_runs_free(&tmp);return 0;
}
int mulib_mkir_spans_runs(const mulib_mkir_spans_t *s,mulib_mkir_runs_t *out){
 return mulib_cdd_mkir_spans_runs_limited(s,SIZE_MAX,SIZE_MAX,out);
}
void mulib_mkir_runs_free(mulib_mkir_runs_t *r){
 mulib_mkir_runs_t empty={NULL,0};
 if(!r)return;
 for(size_t i=0;i<r->count;i++)free(r->items[i].annotations);
 free(r->items);*r=empty;
}
void mulib_mkir_spans_free(mulib_mkir_spans_t *s){
 mulib_mkir_spans_t empty={NULL,0,0,NULL,0,0};
 if(!s)return;
 for(size_t i=0;i<s->count;i++)free(s->items[i].label);
 free(s->items);free(s->text);*s=empty;
}
#ifdef __cplusplus
}
#endif
#endif
#endif
