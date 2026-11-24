/** @file mulib-staymatch.h
 * @brief Bounded exact multi-room bundle search with per-room adult/child occupancy.
 * Define MULIB_IMPL_STAYMATCH once; requires canonical mulib-money.h.
 * All rooms MUST belong to the same property; dates are integer civil-day keys
 * from a common caller-controlled calendar conversion (e.g. epoch days).
 * This kernel does not convert dates, infer bed policies, or fetch availability.
 */
#ifndef MULIB_STAYMATCH_H
#define MULIB_STAYMATCH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-money.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_STAYMATCH_MAX_TYPES 24
#define MULIB_STAYMATCH_MAX_GUESTS_PER_KIND 16
#define MULIB_STAYMATCH_MAX_ROOMS 8
#define MULIB_STAYMATCH_MAX_RESULTS 32

typedef struct mulib_staymatch_request {
    uint64_t property_id;
    int64_t check_in_day, check_out_day; /* half-open [in,out), in<out */
    uint8_t adults, children;
    uint8_t max_rooms;               /* 1..8 */
    uint8_t require_adult_per_room;  /* 0 or 1 */
    char currency[4];                /* uppercase three-letter plus NUL */
    uint8_t decimals;                /* 0..6 */
    size_t max_search_nodes;         /* explicit nonzero complexity budget */
} mulib_staymatch_request_t;
typedef struct mulib_staymatch_room {
    uint64_t property_id;
    uint32_t room_type_id;           /* unique within this supplied list */
    int64_t available_from_day, available_until_day;
    uint8_t adult_capacity, child_capacity, total_capacity;
    uint8_t available_units;         /* number of independently bookable rooms */
    mulib_money_t full_stay_price;   /* price of ONE room for requested stay */
} mulib_staymatch_room_t;
typedef struct mulib_staymatch_bundle {
    mulib_money_t total_price;
    uint8_t room_count;
    uint8_t selected[MULIB_STAYMATCH_MAX_TYPES]; /* aligned with input order */
} mulib_staymatch_bundle_t;
/** Returns 1 on exact search completion, including zero feasible bundles.
 * Returns 0 on invalid input or budget exhaustion. On success only the first
 * *result_count bundles are written; both outputs are unchanged on failure.
 * Bundles rank by ascending price, then room count, then higher selection
 * count at the first differing input index. 1<=result_capacity<=32.
 * Candidate totals above INT64_MAX are outside the representable search space.
 * The node budget counts search states inspected (including bound checks);
 * all preprocessing and per-state work is bounded by the constants above.
 * This function uses fixed local storage; no heap allocation or I/O.
 * Borrowed inputs remain immutable during the call. Output buffers must
 * not overlap one another or any input.
 * Per-room occupancy assignment is solved exactly, not inferred from summed
 * capacities. All rooms must belong to the requested property. This legacy
 * entry point trusts the caller's full-stay price to match both requested
 * dates; use solve_dated to validate the quoted dates separately. */
int mulib_staymatch_solve(const mulib_staymatch_request_t *request,
                          const mulib_staymatch_room_t *rooms,size_t room_count,
                          mulib_staymatch_bundle_t *results,size_t result_capacity,
                          size_t *result_count);

typedef struct mulib_staymatch_dated_room {
    mulib_staymatch_room_t room;       /* inventory coverage and exact-stay price */
    int64_t quoted_check_in_day, quoted_check_out_day;
    int64_t book_from, book_until;    /* Unix seconds, half-open; 0<=from<until */
    uint8_t min_adults;               /* <= adult_capacity; request may raise to 1 */
    uint8_t min_child_age, max_child_age; /* inclusive, 0..17 at check-in */
    uint32_t inventory_group;         /* 0: independent, otherwise shared within property */
    uint8_t inventory_limit;          /* 0 for independent; same positive cap for all in group */
} mulib_staymatch_dated_room_t;
/** Exact-stay variant with date and booking-window checks, per-room ages and
 * adult minima, and shared unit limits across alternative products.
 * Valid but mismatched offers are excluded (never normalized or inferred).
 * Malformed offers, even excluded ones, fail the call.
 * child_ages contains exactly request->children ages at check-in; NULL is
 * allowed only when that count is zero. Inputs are read-only call-scoped
 * borrows; output buffers must not overlap one another or inputs.
 * Same completion/error rules as solve, except ties prefer higher counts
 * at the first differing room_type_id in ascending ID order. The selected[]
 * output is still aligned with input order. */
int mulib_staymatch_solve_dated(const mulib_staymatch_request_t *request,
    int64_t booking_at,const uint8_t *child_ages,size_t child_age_count,
    const mulib_staymatch_dated_room_t *rooms,size_t room_count,
    mulib_staymatch_bundle_t *results,size_t result_capacity,size_t *result_count);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_STAYMATCH) && !defined(MULIB_STAYMATCH_IMPLEMENTED)
#define MULIB_STAYMATCH_IMPLEMENTED
/* FINPRED_AGENT_TODO: Benchmark complexity budgets across realistic inventories; represent connecting rooms, cot/bed constraints and richer product combinability. Maintain exhaustive small-instance oracle.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_STAYMATCH_STRIDE_ (MULIB_STAYMATCH_MAX_GUESTS_PER_KIND+1)
typedef struct mulib_staymatch_ctx_ {
    const mulib_staymatch_request_t *r;
    const mulib_staymatch_room_t *rooms;
    const mulib_staymatch_dated_room_t *dated;
    const uint8_t *ages;
    size_t n, nodes, found, cap;
    int exhausted;
    uint8_t counts[MULIB_STAYMATCH_MAX_TYPES];
    uint8_t order[MULIB_STAYMATCH_MAX_TYPES], tie_order[MULIB_STAYMATCH_MAX_TYPES];
    uint8_t limits[MULIB_STAYMATCH_MAX_TYPES], group[MULIB_STAYMATCH_MAX_TYPES];
    uint8_t group_used[MULIB_STAYMATCH_MAX_TYPES], room_limit;
    unsigned suffix[MULIB_STAYMATCH_MAX_TYPES+1][MULIB_STAYMATCH_MAX_ROOMS+1][3];
    mulib_staymatch_bundle_t best[MULIB_STAYMATCH_MAX_RESULTS];
} mulib_staymatch_ctx_;
static const mulib_staymatch_room_t *mulib_staymatch_room_(const mulib_staymatch_ctx_ *c,
                                                           size_t i){
    return c->dated?&c->dated[i].room:&c->rooms[i];
}
static int mulib_staymatch_occupancy_(const mulib_staymatch_ctx_ *c){
    uint8_t a[MULIB_STAYMATCH_STRIDE_*MULIB_STAYMATCH_STRIDE_]={0};
    uint8_t b[MULIB_STAYMATCH_STRIDE_*MULIB_STAYMATCH_STRIDE_]={0};
    size_t i,u,ad,ch,x,y;
    const size_t A=c->r->adults,C=c->r->children;
    a[0]=1;
    for(i=0;i<c->n;++i)for(u=0;u<c->counts[i];++u){
        const mulib_staymatch_room_t *room=&c->rooms[i];
        memset(b,0,sizeof(b));
        for(ad=0;ad<=A;++ad) for(ch=0;ch<=C;++ch){
            if(!a[ad*MULIB_STAYMATCH_STRIDE_+ch])continue;
            for(x=c->r->require_adult_per_room?1U:0U;
                x<=room->adult_capacity && x<=A-ad; ++x){
                for(y=0;y<=room->child_capacity && y<=C-ch;++y){
                    if(x+y==0 || x+y>room->total_capacity)continue;
                    b[(ad+x)*MULIB_STAYMATCH_STRIDE_+ch+y]=1;
                }
            }
        }
        memcpy(a,b,sizeof(a));
    }
    return a[A*MULIB_STAYMATCH_STRIDE_+C]!=0;
}
/* Exact integral occupancy flow. Each child supplies one unit to an eligible
 * child gate; adults supply A units to adult gates. The gates enforce separate
 * capacity, and each room sends at least one occupant to the sink. Shift each
 * lower bound into node balances before looking for a saturating flow. */
#define MULIB_STAYMATCH_FLOW_N_ (4+MULIB_STAYMATCH_MAX_GUESTS_PER_KIND+3*MULIB_STAYMATCH_MAX_ROOMS)
static int mulib_staymatch_dated_occupancy_(const mulib_staymatch_ctx_ *c){
    unsigned char edges[MULIB_STAYMATCH_FLOW_N_][MULIB_STAYMATCH_FLOW_N_]={0};
    int balance[MULIB_STAYMATCH_FLOW_N_]={0};
    unsigned prev[MULIB_STAYMATCH_FLOW_N_],queue[MULIB_STAYMATCH_FLOW_N_];
    enum { source=0, sink=1, adult=2, total=3, child=4 };
    unsigned n=child+c->r->children,i,j,u,v,head,tail,needed=0,add;
    balance[adult]=c->r->adults;
    balance[total]=-((int)c->r->adults+(int)c->r->children);
    for(j=child;j<n;++j)balance[j]=1;
    for(i=0;i<c->n;++i){
        const mulib_staymatch_dated_room_t *d=&c->dated[i];
        unsigned min=d->min_adults;
        if(!min && c->r->require_adult_per_room)min=1;
        for(u=0;u<c->counts[i];++u){
            unsigned a=n++,k=n++,r=n++;
            edges[adult][a]=(unsigned char)(d->room.adult_capacity-min);
            balance[adult]-=(int)min;
            balance[a]+=(int)min;
            edges[a][r]=d->room.adult_capacity;
            edges[k][r]=d->room.child_capacity;
            edges[r][total]=(unsigned char)(d->room.total_capacity-1);
            --balance[r];
            ++balance[total];
            for(j=0;j<c->r->children;++j)
                if(c->ages[j]>=d->min_child_age&&c->ages[j]<=d->max_child_age)
                    edges[child+j][k]=1;
        }
    }
    if(balance[adult]<0)return 0;
    for(i=adult;i<n;++i){
        if(balance[i]>0){edges[source][i]=(unsigned char)balance[i];needed+=(unsigned)balance[i];}
        if(balance[i]<0)edges[i][sink]=(unsigned char)-balance[i];
    }
    while(needed){
        for(i=0;i<n;++i)prev[i]=n;
        prev[source]=source;head=0;tail=0;queue[tail++]=source;
        while(head<tail && prev[sink]==n){
            u=queue[head++];
            for(v=0;v<n;++v)if(prev[v]==n && edges[u][v]){
                prev[v]=u;queue[tail++]=v;
            }
        }
        if(prev[sink]==n)return 0;
        add=needed;
        for(v=sink;v!=source;v=prev[v])if(edges[prev[v]][v]<add)add=edges[prev[v]][v];
        for(v=sink;v!=source;v=prev[v]){
            edges[prev[v]][v]-=(unsigned char)add;
            edges[v][prev[v]]+=(unsigned char)add;
        }
        needed-=add;
    }
    return 1;
}
static int mulib_staymatch_compare_(const mulib_staymatch_bundle_t *x,
    const mulib_staymatch_bundle_t *y,const mulib_staymatch_ctx_ *c){
    size_t i,j;
    if(x->total_price.minor<y->total_price.minor)return -1;
    if(x->total_price.minor>y->total_price.minor)return 1;
    if(x->room_count<y->room_count)return -1;
    if(x->room_count>y->room_count)return 1;
    for(i=0;i<c->n;++i){
        j=c->tie_order[i];
        if(x->selected[j]!=y->selected[j])return x->selected[j]>y->selected[j]?-1:1;
    }
    return 0;
}
static void mulib_staymatch_submit_(mulib_staymatch_ctx_ *c,uint8_t used,
                                    mulib_money_t price){
    mulib_staymatch_bundle_t item;
    size_t i,pos;
    if(!used||!(c->dated?mulib_staymatch_dated_occupancy_(c):mulib_staymatch_occupancy_(c)))return;
    memset(&item,0,sizeof(item));
    item.room_count=used;item.total_price=price;
    memcpy(item.selected,c->counts,c->n);
    pos=0;
    while(pos<c->found && mulib_staymatch_compare_(&c->best[pos],&item,c)<=0)
        ++pos;
    if(pos>=c->cap)return;
    if(c->found<c->cap)++c->found;
    for(i=c->found-1;i>pos;--i)c->best[i]=c->best[i-1];
    c->best[pos]=item;
}
static void mulib_staymatch_visit_(mulib_staymatch_ctx_ *c,size_t idx,
    uint8_t used,mulib_money_t cost,unsigned adults,unsigned children,unsigned guests,
    unsigned min_adults){
    const mulib_staymatch_room_t *x;
    const mulib_staymatch_dated_room_t *d;
    size_t k,limit,i,g,slots=c->room_limit-used;
    if(c->exhausted)return;
    if(c->nodes>=c->r->max_search_nodes){c->exhausted=1;return;}
    ++c->nodes;
    /* Ignore packing and group limits for the suffix capacity upper bound,
     * but count at most the remaining room slots for each capacity metric.
     * All room prices are nonnegative. An equal-cost branch can still win
     * on room-count or ID, so only prune equal price with strictly more rooms. */
    if(min_adults>c->r->adults ||
       adults+c->suffix[idx][slots][0]<c->r->adults ||
       children+c->suffix[idx][slots][1]<c->r->children ||
       guests+c->suffix[idx][slots][2]<(unsigned)c->r->adults+c->r->children)return;
    if(c->found==c->cap){
        const mulib_staymatch_bundle_t *last=&c->best[c->found-1];
        if(cost.minor>last->total_price.minor ||
           (cost.minor==last->total_price.minor && used>last->room_count))return;
    }
    if(idx==c->n){mulib_staymatch_submit_(c,used,cost);return;}
    i=c->order[idx];g=c->group[i];x=mulib_staymatch_room_(c,i);
    d=c->dated?&c->dated[i]:NULL;
    limit=c->limits[i];
    if(limit>slots)limit=slots;
    if(d && d->inventory_group && limit>(size_t)(d->inventory_limit-c->group_used[g]))
        limit=(size_t)(d->inventory_limit-c->group_used[g]);
    if(x->full_stay_price.minor){
        uint64_t affordable=(uint64_t)((INT64_MAX-cost.minor)/x->full_stay_price.minor);
        if(affordable<limit)limit=(size_t)affordable;
    }
    for(k=0;k<=limit;++k){
        /* Dated search starts with cheap inventory, then tries smaller counts.
         * Legacy search retains input-order tie resolution and traversal. */
        unsigned take=(unsigned)(d?limit-k:k);
        mulib_money_t curr=cost;
        curr.minor+=(int64_t)take*x->full_stay_price.minor;
        c->counts[i]=(uint8_t)take;
        c->group_used[g]+=(uint8_t)take;
        mulib_staymatch_visit_(c,idx+1,(uint8_t)(used+take),curr,
            adults+take*x->adult_capacity,children+take*x->child_capacity,
            guests+take*x->total_capacity,
            min_adults+take*(d&&d->min_adults?d->min_adults:c->r->require_adult_per_room));
        c->group_used[g]-=(uint8_t)take;
        if(c->exhausted)break;
    }
    c->counts[i]=0;
}
static int mulib_staymatch_run_(const mulib_staymatch_request_t *r,
    const mulib_staymatch_room_t *rooms,const mulib_staymatch_dated_room_t *dated,
    int64_t booking,const uint8_t *ages,size_t age_n,size_t n,
    mulib_staymatch_bundle_t *results,size_t cap,size_t *count){
    mulib_staymatch_ctx_ c={0};
    mulib_money_t zero;
    size_t i,j,k,m,slots;
    if(!r||(!rooms&&!dated)||!results||!count||n==0||n>MULIB_STAYMATCH_MAX_TYPES||
       cap==0||cap>MULIB_STAYMATCH_MAX_RESULTS||r->property_id==0||
       r->check_in_day>=r->check_out_day||
       r->adults==0||r->adults>MULIB_STAYMATCH_MAX_GUESTS_PER_KIND||
       r->children>MULIB_STAYMATCH_MAX_GUESTS_PER_KIND||
       r->max_rooms==0||r->max_rooms>MULIB_STAYMATCH_MAX_ROOMS||
       r->require_adult_per_room>1||r->max_search_nodes==0||
       r->currency[3]!=0||
       !mulib_money_make(&zero,0,r->currency,r->decimals))return 0;
    if(dated){
        if(booking<0||age_n!=r->children||(age_n&&!ages))return 0;
        for(i=0;i<age_n;++i)if(ages[i]>17)return 0;
    }
    c.r=r;c.rooms=rooms;c.dated=dated;c.ages=ages;c.n=n;c.cap=cap;
    c.room_limit=r->max_rooms;
    if(c.room_limit>r->adults+r->children)c.room_limit=(uint8_t)(r->adults+r->children);
    if(r->require_adult_per_room && c.room_limit>r->adults)c.room_limit=r->adults;
    for(i=0;i<n;++i){
        const mulib_staymatch_room_t *x=mulib_staymatch_room_(&c,i);
        const mulib_staymatch_dated_room_t *d=dated?&dated[i]:NULL;
        if(x->property_id!=r->property_id || x->room_type_id==0 ||
           x->available_from_day>=x->available_until_day ||
           x->total_capacity==0 ||
           x->adult_capacity>x->total_capacity ||
           x->child_capacity>x->total_capacity ||
           !mulib_money_valid(x->full_stay_price) ||
           x->full_stay_price.minor<0 ||
           x->full_stay_price.decimals!=zero.decimals ||
           memcmp(x->full_stay_price.currency,zero.currency,4)!=0)return 0;
        c.group[i]=(uint8_t)i;
        for(j=0;j<i;++j){
            if(mulib_staymatch_room_(&c,j)->room_type_id==x->room_type_id)return 0;
            if(d && d->inventory_group && d->inventory_group==dated[j].inventory_group){
                if(d->inventory_limit!=dated[j].inventory_limit)return 0;
                c.group[i]=c.group[j];
            }
        }
        if(d && (d->quoted_check_in_day>=d->quoted_check_out_day ||
           d->book_from<0 || d->book_until<=d->book_from ||
           d->min_adults>x->adult_capacity ||
           d->min_child_age>d->max_child_age || d->max_child_age>17 ||
           (d->inventory_group==0)!=(d->inventory_limit==0)))return 0;
        c.limits[i]=x->available_units<c.room_limit?x->available_units:c.room_limit;
        if(x->available_from_day>r->check_in_day || x->available_until_day<r->check_out_day ||
           (r->require_adult_per_room && !x->adult_capacity) ||
           (d && (d->quoted_check_in_day!=r->check_in_day ||
                  d->quoted_check_out_day!=r->check_out_day ||
                  booking<d->book_from || booking>=d->book_until)))c.limits[i]=0;
        c.order[i]=(uint8_t)i;c.tie_order[i]=(uint8_t)i;
    }
    if(dated)for(i=1;i<n;++i){
        const mulib_staymatch_room_t *x=&dated[i].room,*y;
        for(j=i;j>0 && dated[c.tie_order[j-1]].room.room_type_id>x->room_type_id;--j)
            c.tie_order[j]=c.tie_order[j-1];
        c.tie_order[j]=(uint8_t)i;
        for(j=i;j>0;--j){
            y=&dated[c.order[j-1]].room;
            if(y->full_stay_price.minor<x->full_stay_price.minor ||
               (y->full_stay_price.minor==x->full_stay_price.minor &&
                y->room_type_id<x->room_type_id))break;
            c.order[j]=c.order[j-1];
        }
        c.order[j]=(uint8_t)i;
    }
    for(i=n;i>0;--i){
        const mulib_staymatch_room_t *x=mulib_staymatch_room_(&c,c.order[i-1]);
        unsigned limit=c.limits[c.order[i-1]];
        unsigned capacity[3]={x->adult_capacity,x->child_capacity,x->total_capacity};
        for(slots=1;slots<=c.room_limit;++slots)
            for(k=0;k<=slots&&k<=limit;++k)
                for(m=0;m<3;++m){
                    unsigned upper=c.suffix[i][slots-k][m]+(unsigned)k*capacity[m];
                    if(upper>c.suffix[i-1][slots][m])c.suffix[i-1][slots][m]=upper;
                }
    }
    mulib_staymatch_visit_(&c,0,0,zero,0,0,0,0);
    if(c.exhausted)return 0;
    memcpy(results,c.best,c.found*sizeof(*results));
    *count=c.found;
    return 1;
}
int mulib_staymatch_solve(const mulib_staymatch_request_t *r,
    const mulib_staymatch_room_t *rooms,size_t n,mulib_staymatch_bundle_t *results,
    size_t cap,size_t *count){
    return mulib_staymatch_run_(r,rooms,NULL,0,NULL,0,n,results,cap,count);
}
int mulib_staymatch_solve_dated(const mulib_staymatch_request_t *r,
    int64_t booking,const uint8_t *ages,size_t age_n,
    const mulib_staymatch_dated_room_t *rooms,size_t n,mulib_staymatch_bundle_t *results,
    size_t cap,size_t *count){
    return mulib_staymatch_run_(r,NULL,rooms,booking,ages,age_n,n,results,cap,count);
}
#ifdef __cplusplus
}
#endif
#endif
