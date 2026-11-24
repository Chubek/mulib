/* MuLIB: spice-mna. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_MNA in exactly one implementation translation unit.
 * Profile: Dense reference MNA plus sparse row assembly and sparse LU DC/backward Euler API. Not wired into every existing spice device or analysis.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_MNA_H
#define MULIB_SPICE_MNA_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#include "mulib-num-sparse-lu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t n;double *g,*c,*b;} mulib_spice_mna;
static inline int mulib_spice_mna_nodes(const mulib_spice_mna *m,size_t a,size_t b){return m&&m->g&&m->c&&m->b&&(a==MULIB_STORAGE_NONE||a<m->n)&&(b==MULIB_STORAGE_NONE||b<m->n)&&a!=b;}
static inline void mulib_spice_mna_pair(double *p,size_t n,size_t a,size_t b,double v){if(a!=MULIB_STORAGE_NONE)p[a*n+a]+=v;if(b!=MULIB_STORAGE_NONE)p[b*n+b]+=v;if(a!=MULIB_STORAGE_NONE&&b!=MULIB_STORAGE_NONE){p[a*n+b]-=v;p[b*n+a]-=v;}}
int mulib_spice_mna_init(mulib_spice_mna *m,size_t dimension);
void mulib_spice_mna_free(mulib_spice_mna *m);
int mulib_spice_mna_conductance(mulib_spice_mna *m,size_t a,size_t b,double value);
int mulib_spice_mna_capacitance(mulib_spice_mna *m,size_t a,size_t b,double value);
int mulib_spice_mna_current(mulib_spice_mna *m,size_t a,size_t b,double amp);
int mulib_spice_mna_voltage(mulib_spice_mna *m,size_t a,size_t b,size_t branch,double voltage);
int mulib_spice_mna_solve(const mulib_spice_mna *m,double *x,double tolerance);
/* Sparse assembly uses independent row storage, with ground represented by MULIB_STORAGE_NONE. */
typedef struct {size_t n;mulib_num_lu_row *g,*c;double *b;} mulib_spice_sparse_mna;
int mulib_spice_sparse_mna_init(mulib_spice_sparse_mna *m,size_t dimension);
void mulib_spice_sparse_mna_free(mulib_spice_sparse_mna *m);
int mulib_spice_sparse_mna_add(mulib_spice_sparse_mna *m,int mass,size_t row,size_t col,double value);
int mulib_spice_sparse_mna_pair(mulib_spice_sparse_mna *m,int mass,size_t a,size_t b,double value);
int mulib_spice_sparse_mna_voltage(mulib_spice_sparse_mna *m,size_t a,size_t b,size_t branch,double voltage);
int mulib_spice_sparse_mna_current(mulib_spice_sparse_mna *m,size_t a,size_t b,double current);
int mulib_spice_sparse_mna_step(const mulib_spice_sparse_mna *m,double dt,const double *previous,double *out,double tolerance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_MNA) && !defined(MULIB_SPICE_MNA_H_IMPLEMENTED)
#define MULIB_SPICE_MNA_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_mna_init(mulib_spice_mna *m,size_t dimension){if(!m||m->g||m->c||m->b||!dimension)return 0;size_t nn;if(!mulib_storage_size_mul(dimension,dimension,&nn))return 0;mulib_spice_mna t={0};t.n=dimension;t.g=(double*)mulib_storage_alloc(nn,sizeof(double));t.c=(double*)mulib_storage_alloc(nn,sizeof(double));t.b=(double*)mulib_storage_alloc(dimension,sizeof(double));if(!t.g||!t.c||!t.b){mulib_spice_mna_free(&t);return 0;}*m=t;return 1;}
void mulib_spice_mna_free(mulib_spice_mna *m){if(m){free(m->g);free(m->c);free(m->b);memset(m,0,sizeof *m);}}
int mulib_spice_mna_conductance(mulib_spice_mna *m,size_t a,size_t b,double value){if(!mulib_spice_mna_nodes(m,a,b)||!isfinite(value))return 0;mulib_spice_mna_pair(m->g,m->n,a,b,value);return 1;}
int mulib_spice_mna_capacitance(mulib_spice_mna *m,size_t a,size_t b,double value){if(!mulib_spice_mna_nodes(m,a,b)||!isfinite(value)||value<0)return 0;mulib_spice_mna_pair(m->c,m->n,a,b,value);return 1;}
int mulib_spice_mna_current(mulib_spice_mna *m,size_t a,size_t b,double amp){if(!mulib_spice_mna_nodes(m,a,b)||!isfinite(amp))return 0;if(a!=MULIB_STORAGE_NONE)m->b[a]-=amp;if(b!=MULIB_STORAGE_NONE)m->b[b]+=amp;return 1;}
int mulib_spice_mna_voltage(mulib_spice_mna *m,size_t a,size_t b,size_t branch,double voltage){if(!mulib_spice_mna_nodes(m,a,b)||branch>=m->n||branch==a||branch==b||!isfinite(voltage))return 0;if(a!=MULIB_STORAGE_NONE){m->g[a*m->n+branch]+=1;m->g[branch*m->n+a]+=1;}if(b!=MULIB_STORAGE_NONE){m->g[b*m->n+branch]-=1;m->g[branch*m->n+b]-=1;}m->b[branch]+=voltage;return 1;}
int mulib_spice_mna_solve(const mulib_spice_mna *m,double *x,double tolerance){return m&&mulib_num_dense_reference_solve(m->n,m->g,m->b,x,tolerance);}

int mulib_spice_sparse_mna_init(mulib_spice_sparse_mna *m,size_t n){
    if(!m||m->g||m->c||m->b||!n)return 0;
    mulib_spice_sparse_mna t={0};t.n=n;t.g=(mulib_num_lu_row*)mulib_storage_alloc(n,sizeof *t.g);
    t.c=(mulib_num_lu_row*)mulib_storage_alloc(n,sizeof *t.c);t.b=(double*)mulib_storage_alloc(n,sizeof *t.b);
    if(!t.g||!t.c||!t.b){mulib_spice_sparse_mna_free(&t);return 0;}*m=t;return 1;
}
void mulib_spice_sparse_mna_free(mulib_spice_sparse_mna *m){
    if(!m)return;
    for(size_t i=0;i<m->n;i++){if(m->g)free(m->g[i].v);if(m->c)free(m->c[i].v);}
    free(m->g);free(m->c);free(m->b);memset(m,0,sizeof *m);
}
int mulib_spice_sparse_mna_add(mulib_spice_sparse_mna *m,int mass,size_t row,size_t col,double value){
    if(!m||!m->g||!m->c||row>=m->n||col>=m->n||!isfinite(value))return 0;
    mulib_num_lu_row *r=(mass?m->c:m->g)+row;return mulib_num_lu_set(r,col,mulib_num_lu_get(r,col)+value);
}
static inline int mulib_spice_sparse_mna_nodes(const mulib_spice_sparse_mna *m,size_t a,size_t b){
    return m&&m->g&&m->c&&m->b&&a!=b&&(a==MULIB_STORAGE_NONE||a<m->n)&&(b==MULIB_STORAGE_NONE||b<m->n);
}
/* Preallocate every row affected by a pair stamp before modifying values. */
int mulib_spice_sparse_mna_pair(mulib_spice_sparse_mna *m,int mass,size_t a,size_t b,double v){
    if(!mulib_spice_sparse_mna_nodes(m,a,b)||!isfinite(v))return 0;
    mulib_num_lu_row *r=mass?m->c:m->g;
    size_t ids[4]={a,b,a,b},cols[4]={a,b,b,a};double delta[4]={v,v,-v,-v};
    for(unsigned i=0;i<4;i++)if(ids[i]!=MULIB_STORAGE_NONE&&cols[i]!=MULIB_STORAGE_NONE&&!isfinite(mulib_num_lu_get(r+ids[i],cols[i])+delta[i]))return 0;
    if(a!=MULIB_STORAGE_NONE&&(r[a].n>SIZE_MAX-2||!mulib_storage_reserve((void**)&r[a].v,&r[a].cap,r[a].n+2,sizeof *r[a].v)))return 0;
    if(b!=MULIB_STORAGE_NONE&&(r[b].n>SIZE_MAX-2||!mulib_storage_reserve((void**)&r[b].v,&r[b].cap,r[b].n+2,sizeof *r[b].v)))return 0;
    for(unsigned i=0;i<4;i++)if(ids[i]!=MULIB_STORAGE_NONE&&cols[i]!=MULIB_STORAGE_NONE&&!mulib_num_lu_set(r+ids[i],cols[i],mulib_num_lu_get(r+ids[i],cols[i])+delta[i]))return 0;
    return 1;
}
int mulib_spice_sparse_mna_voltage(mulib_spice_sparse_mna *m,size_t a,size_t b,size_t branch,double v){
    if(!mulib_spice_sparse_mna_nodes(m,a,b)||branch>=m->n||branch==a||branch==b||!isfinite(v+m->b[branch]))return 0;
    mulib_num_lu_row *r=m->g;size_t idx[4]={a,branch,b,branch},col[4]={branch,a,branch,b};double d[4]={1,1,-1,-1};
    for(unsigned i=0;i<4;i++)if(idx[i]!=MULIB_STORAGE_NONE&&col[i]!=MULIB_STORAGE_NONE){if(!isfinite(mulib_num_lu_get(r+idx[i],col[i])+d[i])||r[idx[i]].n>SIZE_MAX-2||!mulib_storage_reserve((void**)&r[idx[i]].v,&r[idx[i]].cap,r[idx[i]].n+2,sizeof *r[idx[i]].v))return 0;}
    for(unsigned i=0;i<4;i++)if(idx[i]!=MULIB_STORAGE_NONE&&col[i]!=MULIB_STORAGE_NONE)if(!mulib_num_lu_set(r+idx[i],col[i],mulib_num_lu_get(r+idx[i],col[i])+d[i]))return 0;
    m->b[branch]+=v;return 1;
}
int mulib_spice_sparse_mna_current(mulib_spice_sparse_mna *m,size_t a,size_t b,double v){
    if(!mulib_spice_sparse_mna_nodes(m,a,b)||!isfinite(v)||(a!=MULIB_STORAGE_NONE&&!isfinite(m->b[a]-v))||(b!=MULIB_STORAGE_NONE&&!isfinite(m->b[b]+v)))return 0;
    if(a!=MULIB_STORAGE_NONE)m->b[a]-=v;
    if(b!=MULIB_STORAGE_NONE)m->b[b]+=v;
    return 1;
}
/* dt=0: DC; dt>0: backward Euler; prior vector is not modified. */
int mulib_spice_sparse_mna_step(const mulib_spice_sparse_mna *m,double dt,const double *prev,double *out,double tol){
    if(!m||!m->n||!m->g||!m->c||!m->b||!out||dt<0||!isfinite(dt)||tol<0||!isfinite(tol)||(dt>0&&!mulib_num_all_finite(prev,m->n))||m->n==SIZE_MAX)return 0;
    size_t n=m->n,nnz=0;size_t *ptr=(size_t*)mulib_storage_alloc(n+1,sizeof *ptr),*idx=NULL;double *val=NULL,*b=(double*)mulib_storage_alloc(n,sizeof *b);mulib_num_lu_row *rows=(mulib_num_lu_row*)mulib_storage_alloc(n,sizeof *rows);mulib_num_sparse_lu lu={0};int ok=0;
    if(!ptr||!b||!rows)goto done;
    memcpy(b,m->b,n*sizeof *b);
    for(size_t i=0;i<n;i++){
        for(size_t k=0;k<m->g[i].n;k++){mulib_num_lu_entry e=m->g[i].v[k];if(e.col>=n||!mulib_num_lu_set(rows+i,e.col,mulib_num_lu_get(rows+i,e.col)+e.v))goto done;}
        if(dt>0)for(size_t k=0;k<m->c[i].n;k++){mulib_num_lu_entry e=m->c[i].v[k];if(e.col>=n||!mulib_num_lu_set(rows+i,e.col,mulib_num_lu_get(rows+i,e.col)+e.v/dt))goto done;b[i]+=e.v/dt*prev[e.col];}
        if(rows[i].n>SIZE_MAX-nnz)goto done;
        nnz+=rows[i].n;ptr[i+1]=nnz;
    }
    idx=(size_t*)mulib_storage_alloc(nnz,sizeof *idx);val=(double*)mulib_storage_alloc(nnz,sizeof *val);if(!idx||!val)goto done;
    for(size_t i=0;i<n;i++)for(size_t j=0;j<rows[i].n;j++){idx[ptr[i]+j]=rows[i].v[j].col;val[ptr[i]+j]=rows[i].v[j].v;}
    mulib_num_csr_view a={n,n,nnz,ptr,idx,val};ok=mulib_num_sparse_lu_factor(&a,tol,&lu)&&mulib_num_sparse_lu_solve(&lu,b,out);
 done:mulib_num_sparse_lu_free(&lu);if(rows)for(size_t i=0;i<n;i++)free(rows[i].v);free(rows);free(ptr);free(idx);free(val);free(b);return ok;
}
#ifdef __cplusplus
}
#endif
#endif
