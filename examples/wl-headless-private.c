/* Private Linux/POSIX host: wl_compositor v1, wl_shm v1 and xdg-shell
 * v1 toplevel subset only. No seat, wl_output, positioners/popups, subsurfaces,
 * DMA-BUF or outer display. The fixed software output uses clipped tiles,
 * not a desktop placement policy. Only use controlled local clients. */
#include "wl-headless-private.h"
#include "mulib-glx-scene.h"
#include <time.h>

static int host_now(uint64_t *ns)
{
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return 0;
    *ns=(uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
    return 1;
}

void wl_headless_private_close(wl_headless_private *h)
{
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++)
        mulib_wl_client_free(&h->clients[i]);
    mulib_wl_server_free(&h->server);
    mulib_wl_compositor_free(&h->compositor);
    mgx_surface_free(&h->output);
    if(h->runtime[0]) {
        (void)rmdir(h->runtime);
        h->runtime[0]='\0';
    }
}

int wl_headless_private_open(wl_headless_private *h,const char *parent)
{
    *h=(wl_headless_private){0};
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++)h->clients[i].fd=-1;
    h->server.fd=-1;
    h->compositor.limit=WL_HEADLESS_SURFACES;
    h->compositor.snapshot_limit=1024u*1024u;
    h->quota.limit=(mulib_wl_usage){32,64u*1024u*1024u,128,WL_HEADLESS_SURFACES,
                                   16u*1024u*1024u};
    struct stat st;
    if(!parent||parent[0]!='/'||stat(parent,&st)<0||!S_ISDIR(st.st_mode)||
       (st.st_uid!=0&&st.st_uid!=geteuid())||
       ((st.st_mode&(S_IWGRP|S_IWOTH))&&!(st.st_mode&S_ISVTX))) {
        errno=EINVAL;
        return 0;
    }
    char runtime[sizeof h->runtime], path[sizeof h->server.path];
    int n=snprintf(runtime,sizeof runtime,"%s/mulib-wl.XXXXXX",parent);
    if(n<0||(size_t)n+sizeof "/wayland-0">sizeof path) {
        errno=ENAMETOOLONG;
        return 0;
    }
    if(!mkdtemp(runtime))return 0;
    memcpy(h->runtime,runtime,(size_t)n+1);
    if(chmod(runtime,0700)<0)goto fail;
    memcpy(path,runtime,(size_t)n);
    memcpy(path+n,"/wayland-0",sizeof "/wayland-0");
    uint64_t now;
    if(!host_now(&now)||!mulib_glx_frameclock_init(&h->clock,60000,now)||
       !mulib_wl_server_open(&h->server,path))goto fail;
    errno=ENOMEM;
    if(!mulib_wl_registry_add(&h->server.registry,"wl_compositor",1,
                              mulib_wl_compositor_bind,&h->compositor)||
       !mulib_wl_registry_add(&h->server.registry,"wl_shm",1,mulib_wl_shm_bind,NULL)||
       !mulib_wl_registry_add(&h->server.registry,"xdg_wm_base",1,mulib_wl_xdg_bind,&h->xdg)||
       !mgx_surface_init(&h->output,WL_HEADLESS_W,WL_HEADLESS_H))goto fail;
    mgx_clear(&h->output,WL_HEADLESS_BG);
    return 1;
fail: {
    int saved=errno;
    wl_headless_private_close(h);
    errno=saved;
    return 0;
}
}

static size_t host_collect(wl_headless_private *h,mulib_wl_surface **visible)
{
    size_t n=0;
    for(size_t i=0;i<h->compositor.n;i++) {
        mulib_wl_surface *s=h->compositor.v[i];
        if(s->role!=MULIB_WL_ROLE_TOPLEVEL||!s->current.buffer||!s->configured)continue;
        visible[n]=s;
        if(n>=h->mapped||h->ids[n]!=s->id||h->revisions[n]!=s->revision)
            mulib_glx_frameclock_damage(&h->clock);
        n++;
    }
    if(n!=h->mapped)mulib_glx_frameclock_damage(&h->clock);
    return n;
}

static int host_present(wl_headless_private *h,uint64_t now)
{
    mulib_wl_surface *visible[WL_HEADLESS_SURFACES];
    size_t n=host_collect(h,visible);
    if(!mulib_glx_frameclock_begin(&h->clock,now))return 1;
    mulib_glx_scene scene={0};
    for(size_t i=0;i<n;i++) {
        mulib_glx_scenenode node=mulib_glx_scenenode_default();
        node.x=(double)(i%4)*(WL_HEADLESS_W/4);
        node.y=(double)(i/4)*(WL_HEADLESS_H/4);
        node.clipped=1;
        node.clip=(mulib_wm_rect){0,0,WL_HEADLESS_W/4,WL_HEADLESS_H/4};
        node.buffer=&visible[i]->current.buffer->image;
        if(!mulib_glx_scene_add(&scene,0,&node)) {
            mulib_glx_scene_free(&scene);
            return 0;
        }
    }
    mgx_clear(&h->output,WL_HEADLESS_BG);
    int rendered=mulib_glx_scene_render(&scene,h->output.pixels,
        h->output.stride*(size_t)h->output.h,h->output.stride,h->output.w,h->output.h);
    mulib_glx_scene_free(&scene);
    if(!rendered||!host_now(&now)||!mulib_glx_frameclock_done(&h->clock,now,1))
        return 0;
    h->frames++;
    h->mapped=n;
    for(size_t i=0;i<n;i++) {
        h->ids[i]=visible[i]->id;
        h->revisions[i]=visible[i]->revision;
    }

    /* A callback belongs to a particular connection and committed surface.
     * Done is queued only after painting that surface, not on buffer release. */
    struct {
        mulib_wl_client *client;
        mulib_wl_surface_resource *surface;
    } callbacks[WL_HEADLESS_SURFACES];
    size_t count=0;
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++) {
        mulib_wl_client *c=&h->clients[i];
        for(size_t j=0;c->fd>=0&&j<c->objects.n;j++) {
            mulib_wl_resource *r=&c->objects.v[j];
            if(r->request!=mulib_wl_surface_resource_request)continue;
            mulib_wl_surface_resource *w=(mulib_wl_surface_resource*)r->user;
            if(!w->committed_frames.n)continue;
            for(size_t k=0;k<n;k++)if(visible[k]==w->surface) {
                callbacks[count].client=c;
                callbacks[count++].surface=w;
                break;
            }
        }
    }
    /* frame_done mutates the resource vector, not the heap surface. */
    for(size_t i=0;i<count;i++) {
        mulib_wl_client *c=callbacks[i].client;
        if(!c->failed)(void)mulib_wl_frame_done(&callbacks[i].surface->committed_frames,
                                               c,(uint32_t)(now/1000000));
    }
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++)if(h->clients[i].failed)
        mulib_wl_client_free(&h->clients[i]);
    return 1;
}

int wl_headless_private_step(wl_headless_private *h,int timeout_ms)
{
    uint64_t now;
    if(!host_now(&now))return 0;
    if(h->clock.dirty) {
        uint64_t until=h->clock.next>now?h->clock.next-now:0;
        uint64_t ms=until/1000000+(until%1000000!=0);
        if(timeout_ms<0||ms<(uint64_t)timeout_ms)
            timeout_ms=ms>INT_MAX?INT_MAX:(int)ms;
    }
    struct pollfd p[1+WL_HEADLESS_CLIENTS]={0};
    size_t free_slot=WL_HEADLESS_CLIENTS;
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++) {
        mulib_wl_client *c=&h->clients[i];
        p[1+i].fd=c->fd;
        if(c->fd<0) {
            h->pending_dispatch[i]=0;
            if(free_slot==WL_HEADLESS_CLIENTS)free_slot=i;
            continue;
        }
        if(h->pending_dispatch[i])timeout_ms=0;
        else p[1+i].events=POLLIN;
        if(mulib_wl_client_has_output(c))p[1+i].events|=POLLOUT;
    }
    p[0].fd=free_slot<WL_HEADLESS_CLIENTS?h->server.fd:-1;
    p[0].events=POLLIN;
    if(poll(p,1+WL_HEADLESS_CLIENTS,timeout_ms)<0)return errno==EINTR;
    if(p[0].revents&(POLLERR|POLLHUP|POLLNVAL)) {
        errno=EIO;
        return 0;
    }
    if(p[0].revents&POLLIN) {
        mulib_wl_client *c=&h->clients[free_slot];
        int status=mulib_wl_server_accept(&h->server,c,WL_HEADLESS_OBJECTS);
        if(status<0)return 0;
        if(status>0&&!mulib_wl_client_limits(c,
            (mulib_wl_usage){8,16u*1024u*1024u,32,8,4u*1024u*1024u},&h->quota)) {
            errno=EINVAL;
            return 0;
        }
    }
    for(size_t j=0;j<WL_HEADLESS_CLIENTS;j++) {
        size_t i=(h->turn+j)%WL_HEADLESS_CLIENTS;
        mulib_wl_client *c=&h->clients[i];
        short events=p[1+i].revents;
        if(p[1+i].fd<0)continue;
        int dispatched;
        if((events&(POLLERR|POLLNVAL))||
           ((events&POLLOUT)&&mulib_wl_client_flush(c)<0)||
           (!h->pending_dispatch[i]&&(events&(POLLIN|POLLHUP))&&mulib_wl_client_read(c)<0)||
           (dispatched=mulib_wl_dispatch(c,WL_HEADLESS_BUDGET))<0) {
            mulib_wl_client_free(c);
            h->pending_dispatch[i]=0;
        } else h->pending_dispatch[i]=dispatched==2;
    }
    h->turn=(h->turn+1)%WL_HEADLESS_CLIENTS;
    return host_now(&now)&&host_present(h,now);
}
