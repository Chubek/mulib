/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_SHM_H
#define MULIB_WL_SHM_H
#include "mulib-wl-buffer.h"
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
/* Snapshot untrusted SHM with pread: avoids SIGBUS on malicious truncation. Convert premultiplied ARGB to straight. */
static inline int mulib_wl_shm_snapshot(mulib_wl_buffer *b,int fd,size_t pool,size_t offset,int w,int h,size_t stride,int xrgb){if(!b||fd<0||w<=0||h<=0||(size_t)w>SIZE_MAX/4||stride<(size_t)w*4||stride%4||offset%4||offset>pool||stride>(pool-offset)/(size_t)h||pool>INT32_MAX||(size_t)w>SIZE_MAX/sizeof(uint32_t)/(size_t)h)return 0;struct stat st;if(fstat(fd,&st)<0||st.st_size<0||(uint64_t)st.st_size<pool)return 0;size_t count=(size_t)w*h;if(count>UINT32_C(16777216)){errno=EFBIG;return 0;}uint32_t *p=(uint32_t*)malloc(count*4);if(!p)return 0;for(int y=0;y<h;y++){size_t got=0;while(got<(size_t)w*4){ssize_t n=pread(fd,(unsigned char*)(p+(size_t)y*w)+got,(size_t)w*4-got,(off_t)(offset+(size_t)y*stride+got));if(n<0&&errno==EINTR)continue;if(n<=0){free(p);return 0;}got+=(size_t)n;}}for(size_t i=0;i<count;i++){uint32_t c=p[i];unsigned a=xrgb?255:c>>24;uint32_t v=a<<24;if(a)for(unsigned k=0;k<24;k+=8){unsigned x=(((c>>k)&255)*255+a/2)/a;v|=(x>255?255:x)<<k;}p[i]=v;}memset(b,0,sizeof *b);if(!mulib_glx_clientbuffer_init(&b->image,p,count,(size_t)w,w,h,p,free)){free(p);return 0;}return 1;}
#include "mulib-wl-dispatch.h"

/* Server-side wl_shm v1 resources. A buffer holds the pool even after its
 * protocol object is destroyed. Snapshot at commit, not create_buffer. */
typedef struct {int fd;size_t size;unsigned refs;mulib_wl_account *account;} mulib_wl_shm_pool;
typedef struct {mulib_wl_shm_pool *pool;size_t offset,stride;int w,h,xrgb;unsigned refs;uint32_t id;int alive;} mulib_wl_shm_resource;
static inline void mulib_wl_shm_pool_unref(void *u)
{
    mulib_wl_shm_pool *p=(mulib_wl_shm_pool*)u;
    if(p&&p->refs&&!--p->refs) {
        close(p->fd);
        mulib_wl_account_release(p->account,(mulib_wl_usage){.pools=1,.pool_bytes=p->size});
        free(p);
    }
}
static inline void mulib_wl_shm_resource_unref(mulib_wl_shm_resource *b)
{
    if(b&&b->refs&&!--b->refs) {
        mulib_wl_account_release(b->pool->account,(mulib_wl_usage){.buffers=1});
        mulib_wl_shm_pool_unref(b->pool);
        free(b);
    }
}
static inline void mulib_wl_shm_resource_destroy(void *u){mulib_wl_shm_resource *b=(mulib_wl_shm_resource*)u;b->alive=0;mulib_wl_shm_resource_unref(b);}
static inline int mulib_wl_shm_id(mulib_wl_client *c,uint32_t id){return id>=2&&id<0xff000000u&&!mulib_wl_resource_find(&c->objects,id);}
static inline int mulib_wl_shm_buffer_request(mulib_wl_client *c,mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n){(void)p;if(op||n)return 0;return mulib_wl_resource_remove(&c->objects,r->id)&&mulib_wl_delete_id(c,r->id);}
static inline int mulib_wl_shm_pool_request(mulib_wl_client *c,mulib_wl_resource *r,
        uint16_t op,const unsigned char *p,size_t n)
{
    mulib_wl_shm_pool *pool=(mulib_wl_shm_pool*)r->user;
    if(op==1&&n==0)
        return mulib_wl_resource_remove(&c->objects,r->id)&&mulib_wl_delete_id(c,r->id);
    if(op==2&&n==4) {
        uint32_t size=mulib_wl_u32(p);
        struct stat st;
        if(size<=pool->size||size>INT32_MAX||fstat(pool->fd,&st)<0||
           st.st_size<0||(uint64_t)st.st_size<size)return 0;
        if(!mulib_wl_account_charge(pool->account,
            (mulib_wl_usage){.pool_bytes=size-pool->size}))return 0;
        /* Fold the added bytes into the pool's existing accounting ref. */
        mulib_wl_account_release(pool->account,(mulib_wl_usage){0});
        pool->size=size;
        return 1;
    }
    if(op||n!=24)return 0;
    uint32_t id=mulib_wl_u32(p),offset=mulib_wl_u32(p+4),w=mulib_wl_u32(p+8),
             h=mulib_wl_u32(p+12),stride=mulib_wl_u32(p+16),format=mulib_wl_u32(p+20);
    if(!mulib_wl_shm_id(c,id)||!w||!h||w>INT32_MAX||h>INT32_MAX||stride>INT32_MAX||
       format>1||offset%4||stride%4||stride<(uint64_t)w*4||offset>pool->size||
       (uint64_t)stride*h>pool->size-offset||pool->refs==UINT_MAX)return 0;
    mulib_wl_shm_resource *b=(mulib_wl_shm_resource*)calloc(1,sizeof *b);
    if(!b)return 0;
    if(!mulib_wl_account_charge(pool->account,(mulib_wl_usage){.buffers=1})) {
        free(b);
        return 0;
    }
    *b=(mulib_wl_shm_resource){pool,offset,stride,(int)w,(int)h,format==1,1,id,1};
    pool->refs++;
    if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){
        id,1,"wl_buffer",mulib_wl_shm_buffer_request,b,mulib_wl_shm_resource_destroy})) {
        mulib_wl_shm_resource_unref(b);
        return 0;
    }
    return 1;
}
static inline int mulib_wl_shm_request(mulib_wl_client *c,mulib_wl_resource *r,
        uint16_t op,const unsigned char *p,size_t n)
{
    (void)r;
    if(op||n!=8)return 0;
    uint32_t id=mulib_wl_u32(p),size=mulib_wl_u32(p+4);
    if(!mulib_wl_shm_id(c,id)||!size||size>INT32_MAX)return 0;
    int fd=mulib_wl_client_take_fd(c);
    if(fd<0)return 0;
    struct stat st;
    int flags=fcntl(fd,F_GETFL);
    if(flags<0||(flags&O_ACCMODE)==O_WRONLY||
#ifdef O_PATH
       (flags&O_PATH)||
#endif
       fstat(fd,&st)<0||!S_ISREG(st.st_mode)||st.st_size<0||(uint64_t)st.st_size<size) {
        close(fd);
        return 0;
    }
    mulib_wl_shm_pool *pool=(mulib_wl_shm_pool*)malloc(sizeof *pool);
    if(!pool) {
        close(fd);
        return 0;
    }
    if(!mulib_wl_account_charge(c->account,(mulib_wl_usage){.pools=1,.pool_bytes=size})) {
        close(fd);
        free(pool);
        return 0;
    }
    *pool=(mulib_wl_shm_pool){fd,size,1,c->account};
    if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){
        id,1,"wl_shm_pool",mulib_wl_shm_pool_request,pool,mulib_wl_shm_pool_unref})) {
        mulib_wl_shm_pool_unref(pool);
        return 0;
    }
    return 1;
}
static inline int mulib_wl_shm_bind(mulib_wl_client *c,uint32_t id,uint32_t version,void *u){(void)u;if(version!=1||!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){id,1,"wl_shm",mulib_wl_shm_request,NULL,NULL}))return 0;uint32_t f=0;if(!mulib_wl_client_event(c,id,0,&f,4))return 0;f=1;return mulib_wl_client_event(c,id,0,&f,4);}

#endif
