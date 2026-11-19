/* MuLIB / Amb3r: spice-mna. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_MNA in exactly one implementation translation unit.
 * Profile: Dense reference MNA plus sparse row assembly and sparse LU DC/backward Euler API. Not wired into every existing spice device or analysis.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_MNA_H
#define MULIB_SPICE_MNA_H
#include "mulib-amb3r-base.h"
#include "mulib-num-sparse-lu.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {size_t n;double *g,*c,*b;} ma_mna;
static inline int ma_mna_nodes(const ma_mna *m,size_t a,size_t b){return m&&m->g&&m->c&&m->b&&(a==MA_NONE||a<m->n)&&(b==MA_NONE||b<m->n)&&a!=b;}
static inline void ma_mna_pair(double *p,size_t n,size_t a,size_t b,double v){if(a!=MA_NONE)p[a*n+a]+=v;if(b!=MA_NONE)p[b*n+b]+=v;if(a!=MA_NONE&&b!=MA_NONE){p[a*n+b]-=v;p[b*n+a]-=v;}}
int mulib_spice_mna_init(ma_mna *m,size_t dimension);
void mulib_spice_mna_free(ma_mna *m);
int mulib_spice_mna_conductance(ma_mna *m,size_t a,size_t b,double value);
int mulib_spice_mna_capacitance(ma_mna *m,size_t a,size_t b,double value);
int mulib_spice_mna_current(ma_mna *m,size_t a,size_t b,double amp);
int mulib_spice_mna_voltage(ma_mna *m,size_t a,size_t b,size_t branch,double voltage);
int mulib_spice_mna_solve(const ma_mna *m,double *x,double tolerance);
/* Sparse assembly uses independent row storage, with ground represented by MA_NONE. */
typedef struct {size_t n;ma_lu_row *g,*c;double *b;} ma_sparse_mna;
int mulib_spice_sparse_mna_init(ma_sparse_mna *m,size_t dimension);
void mulib_spice_sparse_mna_free(ma_sparse_mna *m);
int mulib_spice_sparse_mna_add(ma_sparse_mna *m,int mass,size_t row,size_t col,double value);
int mulib_spice_sparse_mna_pair(ma_sparse_mna *m,int mass,size_t a,size_t b,double value);
int mulib_spice_sparse_mna_voltage(ma_sparse_mna *m,size_t a,size_t b,size_t branch,double voltage);
int mulib_spice_sparse_mna_current(ma_sparse_mna *m,size_t a,size_t b,double current);
int mulib_spice_sparse_mna_step(const ma_sparse_mna *m,double dt,const double *previous,double *out,double tolerance);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_MNA) && !defined(MULIB_SPICE_MNA_H_IMPLEMENTED)
#define MULIB_SPICE_MNA_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_mna_init(ma_mna *m,size_t dimension){if(!m||m->g||m->c||m->b||!dimension)return 0;size_t nn;if(!ma_mul(dimension,dimension,&nn))return 0;ma_mna t={0};t.n=dimension;t.g=(double*)ma_alloc(nn,sizeof(double));t.c=(double*)ma_alloc(nn,sizeof(double));t.b=(double*)ma_alloc(dimension,sizeof(double));if(!t.g||!t.c||!t.b){mulib_spice_mna_free(&t);return 0;}*m=t;return 1;}
void mulib_spice_mna_free(ma_mna *m){if(m){free(m->g);free(m->c);free(m->b);memset(m,0,sizeof *m);}}
int mulib_spice_mna_conductance(ma_mna *m,size_t a,size_t b,double value){if(!ma_mna_nodes(m,a,b)||!isfinite(value))return 0;ma_mna_pair(m->g,m->n,a,b,value);return 1;}
int mulib_spice_mna_capacitance(ma_mna *m,size_t a,size_t b,double value){if(!ma_mna_nodes(m,a,b)||!isfinite(value)||value<0)return 0;ma_mna_pair(m->c,m->n,a,b,value);return 1;}
int mulib_spice_mna_current(ma_mna *m,size_t a,size_t b,double amp){if(!ma_mna_nodes(m,a,b)||!isfinite(amp))return 0;if(a!=MA_NONE)m->b[a]-=amp;if(b!=MA_NONE)m->b[b]+=amp;return 1;}
int mulib_spice_mna_voltage(ma_mna *m,size_t a,size_t b,size_t branch,double voltage){if(!ma_mna_nodes(m,a,b)||branch>=m->n||branch==a||branch==b||!isfinite(voltage))return 0;if(a!=MA_NONE){m->g[a*m->n+branch]+=1;m->g[branch*m->n+a]+=1;}if(b!=MA_NONE){m->g[b*m->n+branch]-=1;m->g[branch*m->n+b]-=1;}m->b[branch]+=voltage;return 1;}
int mulib_spice_mna_solve(const ma_mna *m,double *x,double tolerance){return m&&ma_solve(m->n,m->g,m->b,x,tolerance);}

int mulib_spice_sparse_mna_init(ma_sparse_mna *m,size_t n){
    if(!m||m->g||m->c||m->b||!n)return 0;
    ma_sparse_mna t={0};t.n=n;t.g=(ma_lu_row*)ma_alloc(n,sizeof *t.g);
    t.c=(ma_lu_row*)ma_alloc(n,sizeof *t.c);t.b=(double*)ma_alloc(n,sizeof *t.b);
    if(!t.g||!t.c||!t.b){mulib_spice_sparse_mna_free(&t);return 0;}*m=t;return 1;
}
void mulib_spice_sparse_mna_free(ma_sparse_mna *m){
    if(!m)return;for(size_t i=0;i<m->n;i++){if(m->g)free(m->g[i].v);if(m->c)free(m->c[i].v);}
    free(m->g);free(m->c);free(m->b);memset(m,0,sizeof *m);
}
int mulib_spice_sparse_mna_add(ma_sparse_mna *m,int mass,size_t row,size_t col,double value){
    if(!m||!m->g||!m->c||row>=m->n||col>=m->n||!isfinite(value))return 0;
    ma_lu_row *r=(mass?m->c:m->g)+row;return ma_lu_set(r,col,ma_lu_get(r,col)+value);
}
static inline int ma_sparse_mna_nodes(const ma_sparse_mna *m,size_t a,size_t b){
    return m&&m->g&&m->c&&m->b&&a!=b&&(a==MA_NONE||a<m->n)&&(b==MA_NONE||b<m->n);
}
/* Preallocate every row affected by a pair stamp before modifying values. */
int mulib_spice_sparse_mna_pair(ma_sparse_mna *m,int mass,size_t a,size_t b,double v){
    if(!ma_sparse_mna_nodes(m,a,b)||!isfinite(v))return 0;ma_lu_row *r=mass?m->c:m->g;
    size_t ids[4]={a,b,a,b},cols[4]={a,b,b,a};double delta[4]={v,v,-v,-v};
    for(unsigned i=0;i<4;i++)if(ids[i]!=MA_NONE&&cols[i]!=MA_NONE&&!isfinite(ma_lu_get(r+ids[i],cols[i])+delta[i]))return 0;
    if(a!=MA_NONE&&(r[a].n>SIZE_MAX-2||!ma_reserve((void**)&r[a].v,&r[a].cap,r[a].n+2,sizeof *r[a].v)))return 0;
    if(b!=MA_NONE&&(r[b].n>SIZE_MAX-2||!ma_reserve((void**)&r[b].v,&r[b].cap,r[b].n+2,sizeof *r[b].v)))return 0;
    for(unsigned i=0;i<4;i++)if(ids[i]!=MA_NONE&&cols[i]!=MA_NONE&&!ma_lu_set(r+ids[i],cols[i],ma_lu_get(r+ids[i],cols[i])+delta[i]))return 0;return 1;
}
int mulib_spice_sparse_mna_voltage(ma_sparse_mna *m,size_t a,size_t b,size_t branch,double v){
    if(!ma_sparse_mna_nodes(m,a,b)||branch>=m->n||branch==a||branch==b||!isfinite(v+m->b[branch]))return 0;
    ma_lu_row *r=m->g;size_t idx[4]={a,branch,b,branch},col[4]={branch,a,branch,b};double d[4]={1,1,-1,-1};
    for(unsigned i=0;i<4;i++)if(idx[i]!=MA_NONE&&col[i]!=MA_NONE){if(!isfinite(ma_lu_get(r+idx[i],col[i])+d[i])||r[idx[i]].n>SIZE_MAX-2||!ma_reserve((void**)&r[idx[i]].v,&r[idx[i]].cap,r[idx[i]].n+2,sizeof *r[idx[i]].v))return 0;}
    for(unsigned i=0;i<4;i++)if(idx[i]!=MA_NONE&&col[i]!=MA_NONE)if(!ma_lu_set(r+idx[i],col[i],ma_lu_get(r+idx[i],col[i])+d[i]))return 0;
    m->b[branch]+=v;return 1;
}
int mulib_spice_sparse_mna_current(ma_sparse_mna *m,size_t a,size_t b,double v){
    if(!ma_sparse_mna_nodes(m,a,b)||!isfinite(v)||(a!=MA_NONE&&!isfinite(m->b[a]-v))||(b!=MA_NONE&&!isfinite(m->b[b]+v)))return 0;
    if(a!=MA_NONE)m->b[a]-=v;if(b!=MA_NONE)m->b[b]+=v;return 1;
}
/* dt=0: DC; dt>0: backward Euler; prior vector is not modified. */
int mulib_spice_sparse_mna_step(const ma_sparse_mna *m,double dt,const double *prev,double *out,double tol){
    if(!m||!m->n||!m->g||!m->c||!m->b||!out||dt<0||!isfinite(dt)||tol<0||!isfinite(tol)||(dt>0&&!ma_finite(prev,m->n))||m->n==SIZE_MAX)return 0;
    size_t n=m->n,nnz=0;size_t *ptr=(size_t*)ma_alloc(n+1,sizeof *ptr),*idx=NULL;double *val=NULL,*b=(double*)ma_alloc(n,sizeof *b);ma_lu_row *rows=(ma_lu_row*)ma_alloc(n,sizeof *rows);ma_sparse_lu lu={0};int ok=0;
    if(!ptr||!b||!rows)goto done;memcpy(b,m->b,n*sizeof *b);
    for(size_t i=0;i<n;i++){
        for(size_t k=0;k<m->g[i].n;k++){ma_lu_entry e=m->g[i].v[k];if(e.col>=n||!ma_lu_set(rows+i,e.col,ma_lu_get(rows+i,e.col)+e.v))goto done;}
        if(dt>0)for(size_t k=0;k<m->c[i].n;k++){ma_lu_entry e=m->c[i].v[k];if(e.col>=n||!ma_lu_set(rows+i,e.col,ma_lu_get(rows+i,e.col)+e.v/dt))goto done;b[i]+=e.v/dt*prev[e.col];}
        if(rows[i].n>SIZE_MAX-nnz)goto done;nnz+=rows[i].n;ptr[i+1]=nnz;
    }
    idx=(size_t*)ma_alloc(nnz,sizeof *idx);val=(double*)ma_alloc(nnz,sizeof *val);if(!idx||!val)goto done;
    for(size_t i=0;i<n;i++)for(size_t j=0;j<rows[i].n;j++){idx[ptr[i]+j]=rows[i].v[j].col;val[ptr[i]+j]=rows[i].v[j].v;}
    ma_csr a={n,n,nnz,ptr,idx,val};ok=mulib_num_sparse_lu_factor(&a,tol,&lu)&&mulib_num_sparse_lu_solve(&lu,b,out);
 done:mulib_num_sparse_lu_free(&lu);if(rows)for(size_t i=0;i<n;i++)free(rows[i].v);free(rows);free(ptr);free(idx);free(val);free(b);return ok;
}
#ifdef __cplusplus
}
#endif
#endif
