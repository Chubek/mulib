/* Shared numerical and storage primitives for the Amb3r kernel family.
 * C11, standard library + libm. No mandatory third-party dependencies.
 * Zero-initialize owned structs before first use. Use the matching free function.
 * Pointer/length arguments must address valid arrays of the specified size;
 * stateful objects require caller synchronization. MA_NONE is SIZE_MAX.
 * This family exposes an independent ma_* type model; it does not automatically
 * replace or adapt the existing MuLIB EDA/HWIL/Spice/solver ABIs.
 */
#ifndef MULIB_AMB3R_BASE_H
#define MULIB_AMB3R_BASE_H
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
#ifdef __cplusplus
extern "C" {
#endif
#define MA_NONE SIZE_MAX
#define MA_PI 3.141592653589793238462643383279502884
#define MA_MIN(a,b) ((a)<(b)?(a):(b))
#define MA_MAX(a,b) ((a)>(b)?(a):(b))
typedef struct {double x,y;} ma_point;
typedef struct {ma_point lo,hi;} ma_box;
typedef struct {double re,im;} ma_complex;
typedef struct {size_t from,to; double weight;} ma_edge;
typedef struct {size_t rows,cols,nnz; const size_t *ptr,*idx; const double *val;} ma_csr;
typedef struct {double *v; size_t n,cap;} ma_dvec;
typedef struct {unsigned char *v; size_t n,cap;} ma_bytes;
typedef struct {uint64_t state;} ma_rng;
static inline int ma_mul(size_t a,size_t b,size_t *out){if(a && b>SIZE_MAX/a)return 0;*out=a*b;return 1;}
static inline void *ma_alloc(size_t n,size_t z){size_t b;if(!ma_mul(n,z,&b))return NULL;return calloc(b?b:1,1);}
static inline int ma_reserve(void **ptr,size_t *cap,size_t need,size_t z){
 if(need<=*cap)return 1;size_t c=*cap?*cap:8,b;while(c<need){if(c>SIZE_MAX/2){c=need;break;}c*=2;}if(!ma_mul(c,z,&b))return 0;void *p=realloc(*ptr,b);if(!p)return 0;*ptr=p;*cap=c;return 1;}
static inline int ma_push(void **p,size_t *n,size_t *cap,size_t z,const void *item){if(*n==SIZE_MAX||!ma_reserve(p,cap,*n+1,z))return 0;memcpy((unsigned char*)*p+*n*z,item,z);++*n;return 1;}
static inline int ma_bytes_append(ma_bytes *b,const void *src,size_t n){if(!b||(!src&&n)||n>SIZE_MAX-b->n)return 0;if(!ma_reserve((void**)&b->v,&b->cap,b->n+n,1))return 0;if(n)memcpy(b->v+b->n,src,n);b->n+=n;return 1;}
static inline void ma_bytes_free(ma_bytes *b){if(b){free(b->v);memset(b,0,sizeof *b);}}
static inline char *ma_strdup(const char *s){if(!s)return NULL;size_t n=strlen(s);if(n==SIZE_MAX)return NULL;char *p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
static inline int ma_finite(const double *v,size_t n){if(!v&&n)return 0;for(size_t i=0;i<n;i++)if(!isfinite(v[i]))return 0;return 1;}
static inline double ma_dot(const double *a,const double *b,size_t n){double s=0;for(size_t i=0;i<n;i++)s+=a[i]*b[i];return s;}
static inline double ma_norm(const double *v,size_t n){double s=0;for(size_t i=0;i<n;i++)s=hypot(s,v[i]);return s;}
static inline uint64_t ma_rand(ma_rng *r){uint64_t x=r->state?r->state:UINT64_C(0x9e3779b97f4a7c15);x^=x>>12;x^=x<<25;x^=x>>27;r->state=x;return x*UINT64_C(2685821657736338717);}
static inline double ma_uniform(ma_rng *r){return (double)(ma_rand(r)>>11)*0x1p-53;}
static inline ma_complex ma_cadd(ma_complex a,ma_complex b){ma_complex z={a.re+b.re,a.im+b.im};return z;}
static inline ma_complex ma_csub(ma_complex a,ma_complex b){ma_complex z={a.re-b.re,a.im-b.im};return z;}
static inline ma_complex ma_cmul(ma_complex a,ma_complex b){ma_complex z={a.re*b.re-a.im*b.im,a.re*b.im+a.im*b.re};return z;}
static inline ma_complex ma_cdiv(ma_complex a,ma_complex b){double d=b.re*b.re+b.im*b.im;ma_complex z={(a.re*b.re+a.im*b.im)/d,(a.im*b.re-a.re*b.im)/d};return z;}
static inline double ma_cabs(ma_complex a){return hypot(a.re,a.im);}
static inline int ma_csr_valid(const ma_csr *a){if(!a||!a->ptr||(!a->idx&&a->nnz)||(!a->val&&a->nnz)||a->ptr[0]||a->ptr[a->rows]!=a->nnz)return 0;for(size_t i=0;i<a->rows;i++){if(a->ptr[i]>a->ptr[i+1]||a->ptr[i+1]>a->nnz)return 0;for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)if(a->idx[k]>=a->cols||!isfinite(a->val[k]))return 0;}return 1;}
static inline void ma_csr_mv(const ma_csr *a,const double *x,double *y){for(size_t i=0;i<a->rows;i++){double s=0;for(size_t k=a->ptr[i];k<a->ptr[i+1];k++)s+=a->val[k]*x[a->idx[k]];y[i]=s;}}
/* Dense reference solve, copied inputs; used only by bounded reference profiles. */
static inline int ma_solve(size_t n,const double *a,const double *b,double *x,double tol){
 size_t nn;if(!n||!a||!b||!x||!isfinite(tol)||tol<0||!ma_mul(n,n,&nn)||!ma_finite(a,nn)||!ma_finite(b,n))return 0;
 double *m=(double*)ma_alloc(nn,sizeof(double)),*r=(double*)ma_alloc(n,sizeof(double));if(!m||!r){free(m);free(r);return 0;}memcpy(m,a,nn*sizeof(double));memcpy(r,b,n*sizeof(double));int ok=1;
 for(size_t k=0;k<n;k++){size_t p=k;for(size_t i=k+1;i<n;i++)if(fabs(m[i*n+k])>fabs(m[p*n+k]))p=i;if(fabs(m[p*n+k])<=tol){ok=0;break;}if(p!=k){for(size_t j=k;j<n;j++){double t=m[k*n+j];m[k*n+j]=m[p*n+j];m[p*n+j]=t;}double t=r[k];r[k]=r[p];r[p]=t;}for(size_t i=k+1;i<n;i++){double f=m[i*n+k]/m[k*n+k];for(size_t j=k+1;j<n;j++)m[i*n+j]-=f*m[k*n+j];r[i]-=f*r[k];}}
 if(ok){for(size_t i=n;i-->0;){double v=r[i];for(size_t j=i+1;j<n;j++)v-=m[i*n+j]*r[j];r[i]=v/m[i*n+i];}ok=ma_finite(r,n);if(ok)memcpy(x,r,n*sizeof(double));}free(m);free(r);return ok;}
static inline int ma_point_valid(ma_point p){return isfinite(p.x)&&isfinite(p.y);}
static inline double ma_dist(ma_point a,ma_point b){return hypot(a.x-b.x,a.y-b.y);}
static inline double ma_cross(ma_point a,ma_point b,ma_point c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
static inline int ma_box_valid(ma_box b){return ma_point_valid(b.lo)&&ma_point_valid(b.hi)&&b.lo.x<=b.hi.x&&b.lo.y<=b.hi.y;}
static inline int ma_box_hit(ma_box a,ma_box b){return a.lo.x<=b.hi.x&&a.hi.x>=b.lo.x&&a.lo.y<=b.hi.y&&a.hi.y>=b.lo.y;}
static inline double ma_seg_dist(ma_point p,ma_point a,ma_point b){double dx=b.x-a.x,dy=b.y-a.y,d=dx*dx+dy*dy;double t=d?((p.x-a.x)*dx+(p.y-a.y)*dy)/d:0;t=MA_MAX(0,MA_MIN(1,t));return hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);}
static inline int ma_poly_inside(const ma_point *p,size_t n,ma_point q){int in=0;if(!p||n<3)return 0;for(size_t i=0,j=n-1;i<n;j=i++){if(ma_seg_dist(q,p[j],p[i])<=1e-12)return 1;if((p[i].y>q.y)!=(p[j].y>q.y)&&q.x<(p[j].x-p[i].x)*(q.y-p[i].y)/(p[j].y-p[i].y)+p[i].x)in=!in;}return in;}
static inline size_t ma_find(size_t *p,size_t a){while(p[a]!=a){p[a]=p[p[a]];a=p[a];}return a;}
static inline void ma_union(size_t *p,size_t a,size_t b){a=ma_find(p,a);b=ma_find(p,b);if(a!=b){if(a<b)p[b]=a;else p[a]=b;}}
static inline int ma_topo(size_t n,const ma_edge *e,size_t m,size_t *out){if((!e&&m)||(!out&&n))return 0;size_t *d=(size_t*)ma_alloc(n,sizeof(size_t));if(!d)return 0;for(size_t i=0;i<m;i++){if(e[i].from>=n||e[i].to>=n||d[e[i].to]==SIZE_MAX){free(d);return 0;}d[e[i].to]++;}size_t head=0,tail=0;for(size_t i=0;i<n;i++)if(!d[i])out[tail++]=i;while(head<tail){size_t u=out[head++];for(size_t j=0;j<m;j++)if(e[j].from==u&&!--d[e[j].to])out[tail++]=e[j].to;}free(d);return tail==n;}
/* Dijkstra over nonnegative edges, directed; returns 1 even for unreachable vertices. */
static inline int ma_dijkstra(size_t n,const ma_edge *e,size_t m,size_t src,double *dist,size_t *prev){if(src>=n||!dist||!prev||(!e&&m))return 0;for(size_t j=0;j<m;j++)if(e[j].from>=n||e[j].to>=n||e[j].weight<0||!isfinite(e[j].weight))return 0;unsigned char *done=(unsigned char*)ma_alloc(n,1);if(!done)return 0;for(size_t i=0;i<n;i++){dist[i]=INFINITY;prev[i]=MA_NONE;}dist[src]=0;for(size_t k=0;k<n;k++){size_t u=MA_NONE;for(size_t i=0;i<n;i++)if(!done[i]&&(u==MA_NONE||dist[i]<dist[u]))u=i;if(u==MA_NONE||!isfinite(dist[u]))break;done[u]=1;for(size_t j=0;j<m;j++)if(e[j].from==u){size_t v=e[j].to;double d=dist[u]+e[j].weight;if(d<dist[v]){dist[v]=d;prev[v]=u;}}}free(done);return 1;}
/* Four-neighbor A* with Manhattan heuristic, unit edge weights. blocked cells are 1. */
static inline int ma_grid_route(size_t w,size_t h,const unsigned char *blocked,size_t start,size_t goal,size_t *path,size_t cap,size_t *len){size_t n;if(!w||!h||!ma_mul(w,h,&n)||!blocked||!len||start>=n||goal>=n||blocked[start]||blocked[goal])return 0;size_t *g=(size_t*)ma_alloc(n,sizeof(size_t)),*p=(size_t*)ma_alloc(n,sizeof(size_t));unsigned char *closed=(unsigned char*)ma_alloc(n,1);if(!g||!p||!closed){free(g);free(p);free(closed);return 0;}for(size_t i=0;i<n;i++){g[i]=MA_NONE;p[i]=MA_NONE;}g[start]=0;int ok=0;for(size_t iter=0;iter<n;iter++){size_t u=MA_NONE;double best=INFINITY;for(size_t i=0;i<n;i++)if(!closed[i]&&g[i]!=MA_NONE){double f=(double)g[i]+fabs((double)(i%w)-(double)(goal%w))+fabs((double)(i/w)-(double)(goal/w));if(f<best){best=f;u=i;}}if(u==MA_NONE)break;if(u==goal){ok=1;break;}closed[u]=1;size_t nb[4]={u%w?u-1:MA_NONE,u%w+1<w?u+1:MA_NONE,u>=w?u-w:MA_NONE,u/w+1<h?u+w:MA_NONE};for(size_t j=0;j<4;j++){size_t v=nb[j];if(v!=MA_NONE&&!blocked[v]&&!closed[v]&&g[u]+1<g[v]){g[v]=g[u]+1;p[v]=u;}}}if(ok){*len=g[goal]+1;if(!path||cap<*len)ok=0;else{size_t u=goal;for(size_t i=*len;i-->0;){path[i]=u;u=p[u];}}}else *len=0;free(g);free(p);free(closed);return ok;}
static inline int ma_word(const char *s){if(!s||!(isalpha((unsigned char)*s)||*s=='_'))return 0;for(s++;*s;s++)if(!(isalnum((unsigned char)*s)||*s=='_'))return 0;return 1;}
static inline int ma_write_u64(ma_bytes *b,uint64_t v){unsigned char x[8];for(int i=0;i<8;i++)x[i]=(unsigned char)(v>>(8*i));return ma_bytes_append(b,x,8);}
static inline int ma_read_u64(const unsigned char *p,size_t n,size_t *pos,uint64_t *v){if(!p||!pos||!v||*pos>n||n-*pos<8)return 0;*v=0;for(int i=0;i<8;i++)*v|=(uint64_t)p[(*pos)++]<<(8*i);return 1;}
#ifdef __cplusplus
}
#endif
#endif
