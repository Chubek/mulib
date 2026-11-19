/* Deterministic native client: same filesystem socket and fd transport as
 * real Wayland clients, stepped alongside the example's host reactor. */
#include "../examples/wl-headless-private.h"
#include "mulib-wayland-protocol.h"
#include <assert.h>

enum { REG=2, COMPOSITOR=3, SHM=4, BASE=5, SURFACE=6, XDG=7, TOP=8,
       POOL=9, BUFFER=10, FRAME=11, SYNC=12 };

typedef struct {int fd; mway_stream rx;} fixture;

static int inside_clients(wl_headless_private *h)
{
    int n=0;
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++)n+=h->clients[i].fd>=0;
    return n;
}

static void req(fixture *f,uint32_t obj,uint16_t op,const void *args,size_t n)
{
    assert(mulib_wayland_wire_send(f->fd,obj,op,args,n,-1)==0);
}

static mway_frame event(wl_headless_private *h,fixture *f,uint32_t obj,uint16_t op)
{
    mway_frame result;
    for(unsigned step=0;step<100;step++) {
        int r=mulib_wayland_wire_stream_peek(&f->rx,&result);
        assert(r>=0);
        if(r) {
            assert(result.object==obj&&result.opcode==op);
            return result;
        }
        assert(wl_headless_private_step(h,5));
        ssize_t n=mulib_wayland_wire_stream_receive(&f->rx,f->fd);
        assert(n>0||(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK)));
    }
    assert(!"timed out waiting for event");
    return (mway_frame){0};
}

static void consume(fixture *f)
{
    assert(mulib_wayland_wire_stream_consume(&f->rx,0,NULL)==1);
}

static void word_event(wl_headless_private *h,fixture *f,uint32_t obj,uint16_t op,uint32_t value)
{
    mway_frame m=event(h,f,obj,op);
    assert(m.n==4&&mulib_wl_u32(m.args)==value);
    consume(f);
}

static void barrier(wl_headless_private *h,fixture *f)
{
    req(f,1,0,(uint32_t[]){SYNC},4);
    word_event(h,f,SYNC,0,0);
    word_event(h,f,1,1,SYNC);
}

static void burst_and_slots(wl_headless_private *h,fixture *main_client)
{
    fixture peers[WL_HEADLESS_CLIENTS]={0};
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++) {
        peers[i].fd=mulib_wayland_wire_connect(h->server.path);
        assert(peers[i].fd>=0);
        assert(wl_headless_private_step(h,0));
    }
    assert(inside_clients(h)==WL_HEADLESS_CLIENTS);

    mway_bytes burst={0};
    for(uint32_t k=0;k<3*WL_HEADLESS_BUDGET;k++)
        assert(mway_message(&burst,1,0,(uint32_t[]){100+k},4));
    assert(send(peers[0].fd,burst.v,burst.n,MSG_NOSIGNAL)==(ssize_t)burst.n);
    mway_bytes_free(&burst);
    req(main_client,1,0,(uint32_t[]){SYNC},4);
    assert(wl_headless_private_step(h,0));
    assert(h->pending_dispatch[1]&&h->clients[1].rn==24u*WL_HEADLESS_BUDGET);
    assert(mulib_wl_client_has_output(&h->clients[0])); /* Quieter peer made progress, too. */
    word_event(h,main_client,SYNC,0,0);
    word_event(h,main_client,1,1,SYNC);
    for(uint32_t k=0;k<3*WL_HEADLESS_BUDGET;k++) {
        word_event(h,&peers[0],100+k,0,0);
        word_event(h,&peers[0],1,1,100+k);
    }
    assert(!h->pending_dispatch[1]&&!h->clients[1].rn);

    close(peers[0].fd);
    assert(wl_headless_private_step(h,0)&&inside_clients(h)==WL_HEADLESS_CLIENTS-1);
    /* The extra connection was queued at the listener, not active. It can
     * sync once the disconnected client frees a slot. */
    barrier(h,&peers[WL_HEADLESS_CLIENTS-1]);
    assert(inside_clients(h)==WL_HEADLESS_CLIENTS);
    for(size_t i=1;i<WL_HEADLESS_CLIENTS;i++)close(peers[i].fd);
    for(size_t i=0;i<WL_HEADLESS_CLIENTS;i++)mulib_wayland_wire_stream_free(&peers[i].rx);
    assert(wl_headless_private_step(h,0));
    assert(inside_clients(h)==1);
}

static void bind_globals(wl_headless_private *h,fixture *f)
{
    const char *const names[]={"wl_compositor","wl_shm","xdg_wm_base"};
    const uint32_t ids[]={COMPOSITOR,SHM,BASE};
    /* Fragment a legitimate get_registry request before its first full header. */
    mway_bytes bytes={0};
    assert(mway_message(&bytes,1,1,(uint32_t[]){REG},4));
    assert(send(f->fd,bytes.v,3,MSG_NOSIGNAL)==3);
    assert(wl_headless_private_step(h,0));
    assert(h->clients[0].rn==3&&!h->pending_dispatch[0]&&
           !mulib_wl_client_has_output(&h->clients[0]));
    assert(send(f->fd,bytes.v+3,bytes.n-3,MSG_NOSIGNAL)==(ssize_t)(bytes.n-3));
    mway_bytes_free(&bytes);
    for(size_t i=0;i<3;i++) {
        mway_frame m=event(h,f,REG,0);
        const char *iface;
        size_t used;
        assert(m.n>=12&&mulib_wl_string_read(m.args+4,m.n-4,&iface,&used));
        assert(m.n==8+used&&mulib_wl_u32(m.args+4+used)==1&&!strcmp(names[i],iface));
        assert(mulib_wayland_protocol_bind(f->fd,REG,mulib_wl_u32(m.args),iface,1,ids[i]));
        consume(f);
    }
    word_event(h,f,SHM,0,0);
    word_event(h,f,SHM,0,1);
    barrier(h,f); /* Also checks that no unexpected output/seat globals leaked. */
}

static uint32_t configure(wl_headless_private *h,fixture *f)
{
    mway_frame m=event(h,f,TOP,0);
    assert(m.n==12&&mulib_wl_u32(m.args+8)==0);
    consume(f);
    m=event(h,f,XDG,0);
    assert(m.n==4);
    uint32_t serial=mulib_wl_u32(m.args);
    assert(serial);
    consume(f);
    req(f,XDG,4,&serial,4);
    return serial;
}

static void map(fixture *f)
{
    req(f,SURFACE,1,(uint32_t[]){BUFFER,0,0},12);
    req(f,SURFACE,2,(uint32_t[]){0,0,2,1},16);
    req(f,SURFACE,3,(uint32_t[]){FRAME},4);
    req(f,SURFACE,6,NULL,0);
}

static void receive_frame(wl_headless_private *h,fixture *f)
{
    mway_frame m=event(h,f,FRAME,0);
    assert(m.n==4);
    consume(f);
    word_event(h,f,1,1,FRAME);
    barrier(h,f); /* A second frame event for the same callback fails this. */
}

int main(void)
{
    wl_headless_private host;
    const char *parent=getenv("TMPDIR");
    if(!parent||!*parent)parent=access("/tmp/opencode",W_OK)==0?"/tmp/opencode":"/tmp";
    assert(wl_headless_private_open(&host,parent));
    char path[sizeof host.server.path], runtime[sizeof host.runtime];
    strcpy(path,host.server.path);
    strcpy(runtime,host.runtime);
    struct stat st;
    assert(stat(runtime,&st)==0&&S_ISDIR(st.st_mode)&&(st.st_mode&0777)==0700&&
           st.st_uid==geteuid());
    assert(stat(path,&st)==0&&S_ISSOCK(st.st_mode)&&(st.st_mode&0777)==0600);
    fixture f={.fd=mulib_wayland_wire_connect(path)};
    assert(f.fd>=0&&wl_headless_private_step(&host,0));
    assert(fcntl(host.clients[0].fd,F_GETFD)&FD_CLOEXEC);
    burst_and_slots(&host,&f);
    bind_globals(&host,&f);
    req(&f,COMPOSITOR,0,(uint32_t[]){SURFACE},4);
    req(&f,BASE,2,(uint32_t[]){XDG,SURFACE},8);
    req(&f,XDG,1,(uint32_t[]){TOP},4);
    req(&f,SURFACE,6,NULL,0);
    uint32_t first=configure(&host,&f);

    char file[sizeof host.runtime+16];
    assert(snprintf(file,sizeof file,"%s/pixels.XXXXXX",runtime)>0);
    int shm=mkstemp(file);
    assert(shm>=0&&unlink(file)==0);
    uint32_t pixels[]={UINT32_C(0x80800000),UINT32_C(0xff224466)};
    assert(write(shm,pixels,sizeof pixels)==sizeof pixels&&ftruncate(shm,16)==0);
    assert(mulib_wayland_wire_send(f.fd,SHM,0,(uint32_t[]){POOL,sizeof pixels},8,shm)==0);
    req(&f,POOL,2,(uint32_t[]){16},4);
    req(&f,POOL,0,(uint32_t[]){BUFFER,0,2,1,8,0},24);
    req(&f,POOL,1,NULL,0);
    word_event(&host,&f,1,1,POOL); /* The buffer, not the wire pool ID, now retains the FD. */
    assert(host.quota.used.pools==1&&host.quota.used.pool_bytes==16&&
           host.quota.used.buffers==1&&host.quota.used.surfaces==1);

    /* Hold the paint clock until release is observed. The changed client data
     * must not affect the already-committed copy when the host paints. */
    host.clock.next=UINT64_MAX;
    map(&f);
    assert(event(&host,&f,BUFFER,0).n==0);
    consume(&f);
    assert(host.frames==0&&host.output.pixels[0]==WL_HEADLESS_BG);
    assert(host.quota.used.snapshot_bytes==sizeof pixels);
    pixels[1]=UINT32_C(0xff00cc55);
    assert(pwrite(shm,pixels,sizeof pixels,0)==sizeof pixels);
    close(shm);
    host.clock.next=0;
    receive_frame(&host,&f);
    assert(host.mapped==1&&host.output.pixels[0]==UINT32_C(0xff901318)&&
           host.output.pixels[1]==UINT32_C(0xff224466)&&host.output.pixels[2]==WL_HEADLESS_BG);

    host.clock.next=0;
    req(&f,SURFACE,1,(uint32_t[]){0,0,0},12);
    req(&f,SURFACE,6,NULL,0);
    barrier(&host,&f);
    assert(host.mapped==0&&host.output.pixels[0]==WL_HEADLESS_BG&&
           !host.quota.used.snapshot_bytes);
    req(&f,SURFACE,6,NULL,0);
    assert(configure(&host,&f)!=first);
    map(&f);
    assert(event(&host,&f,BUFFER,0).n==0);
    consume(&f);
    receive_frame(&host,&f);
    assert(host.mapped==1&&host.output.pixels[1]==UINT32_C(0xff00cc55));
    mulib_glx_clientbuffer *retained=&host.compositor.v[0]->current.buffer->image;
    assert(mulib_glx_clientbuffer_ref(retained));

    host.clock.next=0;
    req(&f,TOP,0,NULL,0);
    word_event(&host,&f,1,1,TOP);
    assert(!host.mapped&&!host.compositor.v[0]->current.buffer);
    req(&f,XDG,0,NULL,0);
    req(&f,SURFACE,3,(uint32_t[]){FRAME},4); /* Uncommitted callback is discarded. */
    req(&f,SURFACE,0,NULL,0);
    req(&f,BUFFER,0,NULL,0);
    req(&f,BASE,0,NULL,0);
    word_event(&host,&f,1,1,XDG);
    word_event(&host,&f,1,1,FRAME);
    word_event(&host,&f,1,1,SURFACE);
    word_event(&host,&f,1,1,BUFFER);
    word_event(&host,&f,1,1,BASE);
    barrier(&host,&f);
    assert(!host.compositor.n);
    close(f.fd);
    assert(wl_headless_private_step(&host,0)&&!inside_clients(&host));
    assert(!host.quota.used.pools&&!host.quota.used.pool_bytes&&!host.quota.used.buffers&&
           !host.quota.used.surfaces&&host.quota.used.snapshot_bytes==sizeof pixels);
    assert(retained->pixels[1]==UINT32_C(0xff00cc55));
    mulib_glx_clientbuffer_unref(retained);
    assert(!host.quota.used.snapshot_bytes);
    mulib_wayland_wire_stream_free(&f.rx);
    wl_headless_private_close(&host);
    assert(access(path,F_OK)<0&&errno==ENOENT&&access(runtime,F_OK)<0&&errno==ENOENT);
    puts("PASS: private Wayland socket, fairness, SHM/xdg lifecycle, frame/release and CPU pixels");
    return 0;
}
