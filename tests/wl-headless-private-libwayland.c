/* Optional genuine libwayland-client fixture; the test runner generates the
 * xdg-shell client declarations from the installed wayland-protocols XML. */
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct {
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *base;
    unsigned globals, toplevels, configures, releases, frames;
} client_fixture;

static void global(void *user,struct wl_registry *registry,uint32_t name,
                   const char *iface,uint32_t version)
{
    client_fixture *f=user;
    assert(version==1);
    if(!strcmp(iface,"wl_compositor"))
        f->compositor=wl_registry_bind(registry,name,&wl_compositor_interface,1);
    else if(!strcmp(iface,"wl_shm"))
        f->shm=wl_registry_bind(registry,name,&wl_shm_interface,1);
    else if(!strcmp(iface,"xdg_wm_base"))
        f->base=wl_registry_bind(registry,name,&xdg_wm_base_interface,1);
    else assert(!"unexpected global");
    f->globals++;
}

static void global_remove(void *user,struct wl_registry *registry,uint32_t name)
{
    (void)user;
    (void)registry;
    (void)name;
}

static void toplevel_configure(void *user,struct xdg_toplevel *top,
                               int32_t w,int32_t h,struct wl_array *states)
{
    client_fixture *f=user;
    (void)top;
    assert(w==0&&h==0&&states->size==0);
    f->toplevels++;
}

static void toplevel_close(void *user,struct xdg_toplevel *top)
{
    (void)user;
    (void)top;
    assert(!"unexpected close");
}

static void surface_configure(void *user,struct xdg_surface *surface,uint32_t serial)
{
    client_fixture *f=user;
    assert(serial&&f->toplevels==f->configures+1);
    xdg_surface_ack_configure(surface,serial);
    f->configures++;
}

static void release(void *user,struct wl_buffer *buffer)
{
    client_fixture *f=user;
    (void)buffer;
    f->releases++;
}

static void frame_done(void *user,struct wl_callback *callback,uint32_t msec)
{
    client_fixture *f=user;
    (void)msec;
    assert(f->releases==1);
    f->frames++;
    wl_callback_destroy(callback);
}

int main(int argc,char **argv)
{
    assert(argc==2&&argv[1][0]=='/');
    static const struct wl_registry_listener registry_events={global,global_remove};
    static const struct xdg_toplevel_listener top_events={
        .configure=toplevel_configure,.close=toplevel_close
    };
    static const struct xdg_surface_listener surface_events={surface_configure};
    static const struct wl_buffer_listener buffer_events={release};
    static const struct wl_callback_listener frame_events={frame_done};
    client_fixture f={0};
    struct wl_display *display=wl_display_connect(argv[1]);
    assert(display);
    struct wl_registry *registry=wl_display_get_registry(display);
    assert(registry&&wl_registry_add_listener(registry,&registry_events,&f)==0);
    assert(wl_display_roundtrip(display)>=0&&f.globals==3&&f.compositor&&f.shm&&f.base);
    struct wl_surface *surface=wl_compositor_create_surface(f.compositor);
    assert(surface);
    struct xdg_surface *xdg=xdg_wm_base_get_xdg_surface(f.base,surface);
    assert(xdg&&xdg_surface_add_listener(xdg,&surface_events,&f)==0);
    struct xdg_toplevel *top=xdg_surface_get_toplevel(xdg);
    assert(top&&xdg_toplevel_add_listener(top,&top_events,&f)==0);
    xdg_toplevel_set_title(top,"MuLIB libwayland private fixture");
    xdg_toplevel_set_app_id(top,"org.mulib.headless-test");
    wl_surface_commit(surface);
    assert(wl_display_roundtrip(display)>=0&&f.configures==1);

    enum { WIDTH=16, HEIGHT=16, STRIDE=WIDTH*4 };
    uint32_t *pixels;
    const size_t bytes=STRIDE*HEIGHT;
    int fd=memfd_create("mulib-wayland-fixture",MFD_CLOEXEC);
    assert(fd>=0&&ftruncate(fd,(off_t)bytes)==0);
    pixels=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    assert(pixels!=MAP_FAILED);
    for(size_t i=0;i<bytes/sizeof *pixels;i++)pixels[i]=UINT32_C(0x004488cc);
    struct wl_shm_pool *pool=wl_shm_create_pool(f.shm,fd,(int32_t)bytes);
    assert(pool);
    close(fd);
    struct wl_buffer *buffer=wl_shm_pool_create_buffer(pool,0,WIDTH,HEIGHT,
                                                       STRIDE,WL_SHM_FORMAT_XRGB8888);
    assert(buffer&&wl_buffer_add_listener(buffer,&buffer_events,&f)==0);
    wl_shm_pool_destroy(pool);
    struct wl_callback *callback=wl_surface_frame(surface);
    assert(callback&&wl_callback_add_listener(callback,&frame_events,&f)==0);
    wl_surface_attach(surface,buffer,0,0);
    wl_surface_damage(surface,0,0,WIDTH,HEIGHT);
    wl_surface_commit(surface);
    assert(wl_display_roundtrip(display)>=0);
    while(!f.frames)assert(wl_display_dispatch(display)>=0);
    assert(f.frames==1&&f.releases==1);

    xdg_toplevel_destroy(top);
    xdg_surface_destroy(xdg);
    wl_surface_destroy(surface);
    wl_buffer_destroy(buffer);
    xdg_wm_base_destroy(f.base);
    assert(wl_display_roundtrip(display)>=0);
    wl_shm_destroy(f.shm);
    wl_compositor_destroy(f.compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    assert(munmap(pixels,bytes)==0);
    puts("PASS: real libwayland-client xdg toplevel, SHM release and frame");
    return 0;
}
