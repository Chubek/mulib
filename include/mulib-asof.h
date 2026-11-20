/** @file mulib-asof.h
 * @brief Bitemporal point-in-time selection; no look-ahead in historical replay.
 * Define MULIB_IMPL_ASOF exactly once. Caller owns all arrays and supplies
 * valid extents. Input and output arrays must not overlap.
 * Effective and knowledge times use nonnegative Unix seconds. The caller
 * controls clock normalization and must provide actual availability times.
 */
#ifndef MULIB_ASOF_H
#define MULIB_ASOF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_asof_row {
    uint64_t key;         /* nonzero dataset key */
    int64_t effective_at; /* domain timestamp */
    int64_t known_at;     /* when information became available */
    uint32_t revision;   /* tie-break on identical effective and known times */
    double value;
} mulib_asof_row_t;
typedef enum mulib_asof_status {
    MULIB_ASOF_MISSING=0,
    MULIB_ASOF_PRESENT=1,
    MULIB_ASOF_WITHDRAWN=2
} mulib_asof_status_t;
typedef struct mulib_asof_snapshot_entry {
    mulib_asof_status_t status;
    mulib_asof_row_t row;
} mulib_asof_snapshot_entry_t;
/** Return 1 when found, 0 when absent, -1 for invalid input. On 0/-1
 * preserve *out and *original_index. Input need not be sorted. Order eligible
 * rows by latest effective time, then latest known time, then highest
 * revision. Exact ranking ties favor the first input row. */
int mulib_asof_select(const mulib_asof_row_t *rows,size_t n,uint64_t key,
                       int64_t effective_cutoff,int64_t knowledge_cutoff,
                       mulib_asof_row_t *out,size_t *original_index);
/** Point-in-time left join: all keys receive found[] 0/1 and value[] on found.
 * Missing value output is zero. Retains key order (including duplicates) and
 * select's tie rule. Both outputs unchanged on failure. At most 1,000,000
 * rows and 100,000 keys. Sorts a temporary key index, then scans rows with
 * binary key lookups; O(key_count) scratch. */
int mulib_asof_join(const mulib_asof_row_t *rows,size_t n,
                    const uint64_t *keys,size_t key_count,
                    int64_t effective_cutoff,int64_t knowledge_cutoff,
                    double *values,uint8_t *found,size_t output_capacity);
/** Replay into ordered snapshot entries using strictly ascending nonzero keys.
 * Cutoffs are inclusive and independent; for a historical decision pass its
 * timestamp as the knowledge cutoff. `withdrawn` is NULL (no withdrawals) or
 * has n flags (0/1). A winning withdrawal suppresses the key without falling
 * back; a later eligible winner may restore it. A withdrawal only wins when
 * visible by both clocks. Missing entries carry only the requested key; their
 * other row fields are zero. Withdrawals retain provenance but have value +0.
 * Present zero is normalized to +0. A conflicting tie at the winning rank
 * rejects the snapshot rather than depending on input order. All rows must
 * have valid metadata and finite values, even those outside the cutoffs.
 * n may be zero with rows NULL. Other limits are as for join.
 * Returns 1 on success, 0 on invalid input/conflict/OOM; preserves all output
 * on failure. Entries are in-memory fields, not a wire format. */
int mulib_asof_snapshot(const mulib_asof_row_t *rows,size_t n,
                        const uint8_t *withdrawn,const uint64_t *keys,size_t key_count,
                        int64_t effective_cutoff,int64_t knowledge_cutoff,
                        mulib_asof_snapshot_entry_t *out,size_t output_capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ASOF) && !defined(MULIB_ASOF_IMPLEMENTED)
#define MULIB_ASOF_IMPLEMENTED
/* FINPRED_AGENT_TODO: Add persistent revision indexes, ingest-clock/source verification and tamper-evident snapshot evidence; benchmark repeated cutoffs.
 * Remove this FINPRED_AGENT_TODO comment after implementing and validating it. */
#include <math.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_asof_query_ {
    uint64_t key;
    size_t output_index, winner;
    int conflict;
} mulib_asof_query_;
static int mulib_asof_validate_(const mulib_asof_row_t *r){
    return r->key && r->effective_at>=0 && r->known_at>=0 && isfinite(r->value);
}
static int mulib_asof_cmp_rank_(const mulib_asof_row_t *a,const mulib_asof_row_t *b){
    if(a->effective_at!=b->effective_at)return a->effective_at>b->effective_at?1:-1;
    if(a->known_at!=b->known_at)return a->known_at>b->known_at?1:-1;
    return (a->revision>b->revision)-(a->revision<b->revision);
}
static int mulib_asof_cmp_key_(const void *aa,const void *bb){
    const mulib_asof_query_ *a=(const mulib_asof_query_ *)aa;
    const mulib_asof_query_ *b=(const mulib_asof_query_ *)bb;
    return (a->key>b->key)-(a->key<b->key);
}
static void mulib_asof_scan_(const mulib_asof_row_t *r,size_t n,
                    const uint8_t *withdrawn,mulib_asof_query_ *q,size_t nq,
                    int64_t effective,int64_t known,int check_conflicts){
    size_t i;
    for(i=0;i<n;++i){
        size_t lo=0,hi=nq,j;
        int rank;
        if(r[i].effective_at>effective||r[i].known_at>known)continue;
        while(lo<hi){
            size_t mid=lo+(hi-lo)/2;
            if(q[mid].key<r[i].key)lo=mid+1;
            else hi=mid;
        }
        if(lo==nq||q[lo].key!=r[i].key)continue;
        j=q[lo].winner;
        rank=j==SIZE_MAX?1:mulib_asof_cmp_rank_(&r[i],&r[j]);
        if(rank>0){q[lo].winner=i;q[lo].conflict=0;}
        else if(rank==0&&check_conflicts){
            if(withdrawn && withdrawn[i]!=withdrawn[j])q[lo].conflict=1;
            else if((!withdrawn||!withdrawn[i])&&r[i].value!=r[j].value)
                q[lo].conflict=1;
        }
    }
}
int mulib_asof_select(const mulib_asof_row_t *r,size_t n,uint64_t key,
                       int64_t effective,int64_t known,
                       mulib_asof_row_t *out,size_t *index){
    size_t i,winner=0;int have=0;
    if(!r||!out||!index||key==0||effective<0||known<0||n==0||n>1000000)return -1;
    for(i=0;i<n;++i){
        if(!mulib_asof_validate_(&r[i]))return -1;
        if(r[i].key!=key||r[i].effective_at>effective||r[i].known_at>known)continue;
        if(!have||mulib_asof_cmp_rank_(&r[i],&r[winner])>0){
            winner=i;have=1;
        }
    }
    if(!have)return 0;
    *out=r[winner];*index=winner;return 1;
}
int mulib_asof_join(const mulib_asof_row_t *r,size_t n,
                    const uint64_t *keys,size_t nk,int64_t effective,int64_t known,
                    double *values,uint8_t *found,size_t cap){
    size_t i,k=SIZE_MAX;
    mulib_asof_query_ *q;
    if(!r||!keys||!values||!found||!nk||nk>cap||nk>100000||
       !n||n>1000000||effective<0||known<0||nk>SIZE_MAX/sizeof(*q))return 0;
    for(i=0;i<n;++i)if(!mulib_asof_validate_(&r[i]))return 0;
    for(i=0;i<nk;++i)if(!keys[i])return 0;
    q=(mulib_asof_query_ *)malloc(nk*sizeof(*q));
    if(!q)return 0;
    for(i=0;i<nk;++i)q[i]=(mulib_asof_query_){keys[i],i,SIZE_MAX,0};
    qsort(q,nk,sizeof(*q),mulib_asof_cmp_key_);
    mulib_asof_scan_(r,n,NULL,q,nk,effective,known,0);
    for(i=0;i<nk;++i){
        if(i==0||q[i].key!=q[i-1].key)k=q[i].winner;
        values[q[i].output_index]=k==SIZE_MAX?0:r[k].value;
        found[q[i].output_index]=(uint8_t)(k!=SIZE_MAX);
    }
    free(q);return 1;
}
int mulib_asof_snapshot(const mulib_asof_row_t *r,size_t n,
                    const uint8_t *withdrawn,const uint64_t *keys,size_t nk,
                    int64_t effective,int64_t known,mulib_asof_snapshot_entry_t *out,size_t cap){
    size_t i;
    mulib_asof_query_ *q;
    if((n&&!r)||!keys||!out||!nk||nk>cap||nk>100000||
       n>1000000||effective<0||known<0||nk>SIZE_MAX/sizeof(*q))return 0;
    for(i=0;i<n;++i)if(!mulib_asof_validate_(&r[i])||(withdrawn&&withdrawn[i]>1))return 0;
    for(i=0;i<nk;++i)if(!keys[i]||(i&&keys[i]<=keys[i-1]))return 0;
    q=(mulib_asof_query_ *)malloc(nk*sizeof(*q));
    if(!q)return 0;
    for(i=0;i<nk;++i)q[i]=(mulib_asof_query_){keys[i],i,SIZE_MAX,0};
    mulib_asof_scan_(r,n,withdrawn,q,nk,effective,known,1);
    for(i=0;i<nk;++i)if(q[i].conflict){free(q);return 0;}
    for(i=0;i<nk;++i){
        size_t j=q[i].winner;
        mulib_asof_snapshot_entry_t entry={MULIB_ASOF_MISSING,{q[i].key,0,0,0,0}};
        if(j!=SIZE_MAX){
            entry.status=withdrawn&&withdrawn[j]?MULIB_ASOF_WITHDRAWN:MULIB_ASOF_PRESENT;
            entry.row=r[j];
            if(entry.status==MULIB_ASOF_WITHDRAWN||entry.row.value==0)entry.row.value=0;
        }
        out[i]=entry;
    }
    free(q);return 1;
}
#ifdef __cplusplus
}
#endif
#endif
