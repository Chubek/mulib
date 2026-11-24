/** @file mulib-orderbook.h
 * @brief Bounded single-instrument level-2 price/quantity book, exact sequence.
 * Define MULIB_IMPL_ORDERBOOK once. Caller owns level storage; fixed price
 * minor unit and lot unit are selected by the host (not encoded here).
 * No transport, trade matching or venue-specific action semantics.
 * The host must own sequence recovery and serialize all storage accesses.
 */
#ifndef MULIB_ORDERBOOK_H
#define MULIB_ORDERBOOK_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_orderbook_side {MULIB_ORDERBOOK_BID=1,MULIB_ORDERBOOK_ASK=2} mulib_orderbook_side_t;
typedef struct mulib_orderbook_level {
    mulib_orderbook_side_t side;
    int64_t price_minor; /* positive integer; single-currency book */
    uint64_t quantity;   /* positive; zero only in update to delete */
} mulib_orderbook_level_t;
typedef struct mulib_orderbook {
    mulib_orderbook_level_t *levels; /* borrowed, live through all book calls */
    size_t count,capacity;
    uint64_t last_sequence; /* init to 0, first update must be 1 */
} mulib_orderbook_t;
int mulib_orderbook_init(mulib_orderbook_t *book,
                         mulib_orderbook_level_t *storage,size_t capacity);
/** Replace a previously initialized book with a full validated snapshot,
 * in bid-descending then ask-ascending order without duplicate levels.
 * `levels` may overlap book storage; n=0 permits NULL. Equal best bid/ask
 * is allowed. The host authorizes snapshot sequence and any rewind/reset.
 * Returns 1 on success; otherwise book and storage are unchanged. */
int mulib_orderbook_load_snapshot(mulib_orderbook_t *book,uint64_t last_sequence,
                                  const mulib_orderbook_level_t *levels,size_t n);
/** Apply exact next sequence atomically; no mutation if failed. Rejects
 * deleting missing levels and crossed books (best bid > best ask).
 * Price/time priority within one price level is NOT modeled. */
int mulib_orderbook_apply(mulib_orderbook_t *book,uint64_t sequence,
                          mulib_orderbook_level_t update);
/** Return best quote on side; 1 present, 0 absent, -1 invalid.
 * Only writes *out on 1; out must not overlap storage or book. */
int mulib_orderbook_best(const mulib_orderbook_t *book,
                         mulib_orderbook_side_t side,mulib_orderbook_level_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ORDERBOOK) && !defined(MULIB_ORDERBOOK_IMPLEMENTED)
#define MULIB_ORDERBOOK_IMPLEMENTED
/* FINPRED_AGENT_TODO: Specify venue-dependent reset/incremental formats, checksums and trading states; benchmark throughput and tune only against measured workloads.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_orderbook_cmp_(mulib_orderbook_level_t a,mulib_orderbook_level_t b){
    if(a.side!=b.side)return a.side<b.side?-1:1;
    if(a.price_minor==b.price_minor)return 0;
    if(a.side==MULIB_ORDERBOOK_BID)return a.price_minor>b.price_minor?-1:1;
    return a.price_minor<b.price_minor?-1:1;
}
static int mulib_orderbook_side_valid_(mulib_orderbook_side_t side){
    return side==MULIB_ORDERBOOK_BID||side==MULIB_ORDERBOOK_ASK;
}
static int mulib_orderbook_storage_valid_(const mulib_orderbook_t *b){
    return b&&b->levels&&b->capacity&&b->capacity<=4096&&b->count<=b->capacity;
}
static int mulib_orderbook_validate_levels_(const mulib_orderbook_level_t *p,size_t n,
                                             size_t *first_ask){
    size_t i;
    *first_ask=n;
    for(i=0;i<n;++i){
        if(!mulib_orderbook_side_valid_(p[i].side)||p[i].price_minor<=0||!p[i].quantity||
           (i&&mulib_orderbook_cmp_(p[i-1],p[i])>=0))return 0;
        if(p[i].side==MULIB_ORDERBOOK_ASK&&*first_ask==n)*first_ask=i;
    }
    return !*first_ask||*first_ask==n||p[0].price_minor<=p[*first_ask].price_minor;
}
int mulib_orderbook_init(mulib_orderbook_t *b,mulib_orderbook_level_t *storage,
                         size_t cap){
    if(!b||!storage||cap==0||cap>4096)return 0;
    b->levels=storage;b->count=0;b->capacity=cap;b->last_sequence=0;return 1;
}
int mulib_orderbook_load_snapshot(mulib_orderbook_t *b,uint64_t seq,
                                  const mulib_orderbook_level_t *p,size_t n){
    size_t first_ask;
    if(!mulib_orderbook_storage_valid_(b)||n>b->capacity||(n&&!p)||
       !mulib_orderbook_validate_levels_(p,n,&first_ask))return 0;
    if(n)memmove(b->levels,p,n*sizeof(*p));
    b->count=n;b->last_sequence=seq;return 1;
}
int mulib_orderbook_apply(mulib_orderbook_t *b,uint64_t seq,
                          mulib_orderbook_level_t upd){
    mulib_orderbook_level_t *p;
    size_t k,n,first_ask;
    int found;
    if(!mulib_orderbook_storage_valid_(b)||b->last_sequence==UINT64_MAX||
       seq!=b->last_sequence+1||!mulib_orderbook_side_valid_(upd.side)||
       upd.price_minor<=0||!mulib_orderbook_validate_levels_(b->levels,b->count,&first_ask))return 0;
    p=b->levels;n=b->count;
    for(k=0;k<n&&mulib_orderbook_cmp_(p[k],upd)<0;++k){}
    found=k<n&&mulib_orderbook_cmp_(p[k],upd)==0;
    if(!upd.quantity){
        if(!found)return 0;
        if(k+1<n)memmove(p+k,p+k+1,(n-k-1)*sizeof(*p));
        --n;
    }else{
        if(!found&&n==b->capacity)return 0;
        /* Only a new bid above the best ask, or a new ask below the best bid,
         * can cross an already validated book. */
        if((upd.side==MULIB_ORDERBOOK_BID&&first_ask<n&&
            upd.price_minor>p[first_ask].price_minor)||
           (upd.side==MULIB_ORDERBOOK_ASK&&first_ask&&upd.price_minor<p[0].price_minor))return 0;
        if(found)p[k].quantity=upd.quantity;
        else{
            if(k<n)memmove(p+k+1,p+k,(n-k)*sizeof(*p));
            p[k]=upd;++n;
        }
    }
    b->count=n;b->last_sequence=seq;return 1;
}
int mulib_orderbook_best(const mulib_orderbook_t *b,
                         mulib_orderbook_side_t side,mulib_orderbook_level_t *out){
    size_t ask;
    if(!out||!mulib_orderbook_storage_valid_(b)||!mulib_orderbook_side_valid_(side)||
       !mulib_orderbook_validate_levels_(b->levels,b->count,&ask))return -1;
    if(side==MULIB_ORDERBOOK_BID){
        if(!ask)return 0;
        *out=b->levels[0];
    }else{
        if(ask==b->count)return 0;
        *out=b->levels[ask];
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif
