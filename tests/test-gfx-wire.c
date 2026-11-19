/* Socketpair-only native wire/backend regression tests: no display or devices. */
#include "mulib-wayland-protocol.h"
#include "mulib-wayland-shm.h"
#include "mulib-wayland-xdg.h"
#include "mulib-wayland-seat.h"
#include "mulib-wayland-data.h"
#include "mulib-wayland-dmabuf.h"
#include "mulib-wayland-textinput.h"
#include "mulib-x11-events.h"
#include "mulib-x11-image.h"
#include "mulib-x11-selection.h"
#include "mulib-x11-window.h"
#include "mulib-x11-wm.h"
#include "mulib-glx-backend-wayland.h"
#include "mulib-glx-backend-x11.h"
#include <assert.h>
#include <dirent.h>
#include <sys/wait.h>

static void pair_open(int fd[2]) {
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
    for(int i=0;i<2;i++) {
        int flags=fcntl(fd[i],F_GETFL);
        assert(flags>=0&&fcntl(fd[i],F_SETFL,flags|O_NONBLOCK)==0);
    }
    int small=1024;
    assert(setsockopt(fd[0],SOL_SOCKET,SO_SNDBUF,&small,sizeof small)==0);
}
static void pair_close(int fd[2]) { assert(close(fd[0])==0); assert(close(fd[1])==0); }
static size_t open_fds(void) {
    DIR *d=opendir("/proc/self/fd");
    assert(d);
    size_t n=0;
    struct dirent *entry;
    while((entry=readdir(d))!=NULL)
        if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,".."))n++;
    assert(closedir(d)==0);
    return n;
}
static void send_rights(int fd,const void *p,size_t n,int source,size_t count) {
    assert(count<=40&&n);
    union { struct cmsghdr align; char data[CMSG_SPACE(40*sizeof(int))]; } control;
    memset(&control,0,sizeof control);
    struct iovec iov={(void *)p,n};
    struct msghdr msg={0};
    msg.msg_iov=&iov; msg.msg_iovlen=1;
    msg.msg_control=control.data; msg.msg_controllen=CMSG_SPACE(count*sizeof(int));
    struct cmsghdr *c=CMSG_FIRSTHDR(&msg);
    c->cmsg_level=SOL_SOCKET; c->cmsg_type=SCM_RIGHTS;
    c->cmsg_len=CMSG_LEN(count*sizeof(int));
    for(size_t i=0;i<count;i++)memcpy((char *)CMSG_DATA(c)+i*sizeof(int),&source,sizeof source);
    assert(sendmsg(fd,&msg,MSG_NOSIGNAL)==(ssize_t)n);
}
static void read_exact(int fd,void *p,size_t n) {
    size_t offset=0;
    assert(mx11_read_step(fd,p,n,&offset)==1&&offset==n);
}
static void assert_empty(int fd) {
    unsigned char b;
    assert(recv(fd,&b,1,MSG_DONTWAIT)<0&&(errno==EAGAIN||errno==EWOULDBLOCK));
}
static void test_wayland_frames(void) {
    mway_bytes b={0};
    uint32_t args[]={11,22};
    assert(mway_message(&b,9,4,args,sizeof args));
    assert(mway_message(&b,7,2,NULL,0));
    mway_frame f={.object=99};
    for(size_t i=0;i<16;i++) {
        assert(mulib_wayland_wire_parse(b.v,i,&f)==0);
        assert(f.object==99);
    }
    assert(mulib_wayland_wire_parse(b.v,b.n,&f)==1);
    assert(f.object==9&&f.opcode==4&&f.n==8&&f.consumed==16);
    assert(!memcmp(f.args,args,sizeof args));
    assert(mulib_wayland_wire_parse(b.v+f.consumed,8,&f)==1&&f.object==7);
    uint32_t bad[]={0,8u<<16};
    assert(mulib_wayland_wire_parse(bad,8,&f)==-1&&errno==EPROTO);
    bad[0]=1; bad[1]=7u<<16;
    assert(mulib_wayland_wire_parse(bad,8,&f)==-1);
    bad[1]=10u<<16;
    assert(mulib_wayland_wire_parse(bad,8,&f)==-1);
    assert(mulib_wayland_wire_parse(NULL,8,&f)==-1&&errno==EINVAL);
    assert(mulib_wayland_wire_parse(b.v,8,NULL)==-1);
    size_t old=b.n;
    assert(!mway_message(&b,1,0,NULL,4)&&b.n==old);
    assert(!mway_message(&b,1,0,args,65528)&&b.n==old);
    mway_bytes_free(&b);
}
static void test_wayland_partial_send(void) {
    size_t baseline=open_fds();
    int fd[2],pipefd[2];
    pair_open(fd);
    assert(pipe(pipefd)==0);
    size_t n=65524;
    unsigned char *args=(unsigned char *)malloc(n);
    assert(args);
    for(size_t i=0;i<n;i++)args[i]=(unsigned char)(i*13);
    mway_tx tx={0};
    assert(mulib_wayland_wire_tx_begin(&tx,8,3,args,n,pipefd[0]));
    assert(tx.has_fd&&(fcntl(tx.sendfd,F_GETFD)&FD_CLOEXEC));
    assert(close(pipefd[0])==0); /* The pending request owns a duplicate. */
    int r=mulib_wayland_wire_tx_step(&tx,fd[0]);
    assert(r==0&&tx.sent>0&&tx.sent<tx.bytes.n&&!tx.has_fd);
    mway_bytes got={0};
    int received=-1;
    size_t descriptors=0;
    for(unsigned turns=0;;turns++) {
        assert(turns<10000);
        unsigned char bytes[8192];
        int fds[4];
        size_t fd_n=0;
        ssize_t k=mulib_wayland_wire_receive_flags(fd[1],bytes,sizeof bytes,
                                                    fds,4,&fd_n,MSG_DONTWAIT);
        if(k<0)assert(errno==EAGAIN||errno==EWOULDBLOCK);
        else {
            assert(k>0&&mway_append(&got,bytes,(size_t)k));
            for(size_t j=0;j<fd_n;j++) {
                assert(received==-1);
                received=fds[j]; descriptors++;
                assert(fcntl(received,F_GETFD)&FD_CLOEXEC);
            }
        }
        if(r!=1)r=mulib_wayland_wire_tx_step(&tx,fd[0]);
        assert(r>=0);
        if(r==1&&got.n==tx.bytes.n)break;
    }
    assert(descriptors==1&&received>=0);
    assert(got.n==n+8&&!memcmp(got.v,tx.bytes.v,got.n));
    assert(mulib_wayland_wire_tx_step(&tx,fd[0])==1);
    assert(write(pipefd[1],"X",1)==1);
    unsigned char x=0;
    assert(read(received,&x,1)==1&&x=='X');
    assert(close(received)==0&&close(pipefd[1])==0);
    free(args); mway_bytes_free(&got); mulib_wayland_wire_tx_free(&tx);
    pair_close(fd);
    assert(open_fds()==baseline);
}
static void test_wayland_receive_ownership(void) {
    size_t baseline=open_fds();
    int fd[2],pipefd[2];
    pair_open(fd);
    assert(pipe(pipefd)==0);
    uint32_t msg[]={4,8u<<16};
    unsigned char buf[32];
    int fds[32];
    size_t fd_n=777;
    size_t before=open_fds();
    send_rights(fd[0],msg,sizeof msg,pipefd[0],2);
    assert(mulib_wayland_wire_receive(fd[1],buf,sizeof buf,fds,1,&fd_n)==-1);
    assert(fd_n==0&&errno==EMSGSIZE&&open_fds()==before);
    send_rights(fd[0],msg,sizeof msg,pipefd[0],40); /* MSG_CTRUNC, not just output-cap overflow. */
    assert(mulib_wayland_wire_receive(fd[1],buf,sizeof buf,fds,32,&fd_n)==-1);
    assert(fd_n==0&&errno==EMSGSIZE&&open_fds()==before);
    send_rights(fd[0],msg,sizeof msg,pipefd[0],1);
    assert(mulib_wayland_wire_receive(fd[1],buf,0,fds,32,&fd_n)==-1&&fd_n==0);
    assert(mulib_wayland_wire_receive(fd[1],buf,sizeof buf,NULL,0,&fd_n)==-1);
    assert(errno==EMSGSIZE&&fd_n==0&&open_fds()==before);
    assert(mulib_wayland_wire_receive(fd[1],buf,sizeof buf,NULL,1,&fd_n)==-1&&errno==EINVAL);
    assert(mulib_wayland_wire_receive(fd[1],buf,sizeof buf,fds,32,NULL)==-1);
    assert(close(pipefd[0])==0&&close(pipefd[1])==0);
    pair_close(fd);
    assert(open_fds()==baseline);
}
static void test_wayland_stream(void) {
    size_t baseline=open_fds();
    int fd[2],pipefd[2];
    pair_open(fd);
    assert(pipe(pipefd)==0);
    mway_bytes b={0};
    uint32_t arg=42;
    assert(mway_message(&b,2,0,&arg,4)&&mway_message(&b,3,1,NULL,0));
    mway_stream stream={0};
    mway_frame f;
    assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==-1&&errno==EAGAIN);
    send_rights(fd[0],b.v,3,pipefd[0],1);
    assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==3);
    assert(stream.fd_n==1&&mulib_wayland_wire_stream_peek(&stream,&f)==0);
    assert(send(fd[0],b.v+3,b.n-3,MSG_NOSIGNAL)==(ssize_t)(b.n-3));
    assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==(ssize_t)(b.n-3));
    assert(mulib_wayland_wire_stream_peek(&stream,&f)==1&&f.object==2);
    int transferred[2]={-1,-1};
    assert(mulib_wayland_wire_stream_consume(&stream,2,transferred)==0);
    assert(transferred[0]==-1&&stream.fd_n==1);
    assert(mulib_wayland_wire_stream_consume(&stream,1,transferred)==1);
    assert(transferred[0]>=0&&(fcntl(transferred[0],F_GETFD)&FD_CLOEXEC));
    assert(close(transferred[0])==0);
    assert(mulib_wayland_wire_stream_peek(&stream,&f)==1&&f.object==3);
    assert(mulib_wayland_wire_stream_consume(&stream,0,NULL)==1);
    assert(close(fd[0])==0);
    assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==0);
    assert(mulib_wayland_wire_stream_peek(&stream,&f)==0);
    mulib_wayland_wire_stream_free(&stream); mway_bytes_free(&b);
    assert(close(fd[1])==0&&close(pipefd[0])==0&&close(pipefd[1])==0);
    assert(open_fds()==baseline);
}
static void test_wayland_stream_errors(void) {
    size_t baseline=open_fds();
    for(int which=0;which<6;which++) {
        int fd[2],pipefd[2];
        pair_open(fd); assert(pipe(pipefd)==0);
        mway_stream stream={0};
        mway_frame f;
        uint32_t msg[]={which==1?0u:4u,8u<<16};
        if(which==0) { /* Truncated header + queued descriptor at EOF. */
            send_rights(fd[0],msg,3,pipefd[0],1);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==3);
        } else if(which==1) {
            send_rights(fd[0],msg,8,pipefd[0],1);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==8);
        } else if(which==2) {
            stream.max_bytes=4;
            send_rights(fd[0],msg,8,pipefd[0],1);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==4);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==-1&&errno==EMSGSIZE);
        } else if(which==3) { /* Frame signature needs an FD that never arrives. */
            assert(send(fd[0],msg,8,MSG_NOSIGNAL)==8);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==8);
        } else if(which==4) { /* New ancillary truncation closes both old and new FDs. */
            send_rights(fd[0],msg,3,pipefd[0],1);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==3);
            send_rights(fd[0],msg,8,pipefd[0],40);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==-1&&errno==EMSGSIZE);
        } else { /* Reject over-limit declared size without waiting for payload. */
            stream.max_bytes=8; msg[1]=12u<<16;
            send_rights(fd[0],msg,8,pipefd[0],1);
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==8);
            assert(mulib_wayland_wire_stream_peek(&stream,&f)==-1&&errno==EMSGSIZE);
        }
        assert(close(fd[0])==0);
        if(which==0||which==3) {
            assert(mulib_wayland_wire_stream_receive(&stream,fd[1])==0);
            if(which==0)assert(mulib_wayland_wire_stream_peek(&stream,&f)==-1&&errno==EPROTO);
            else {
                int unused=-1;
                assert(mulib_wayland_wire_stream_consume(&stream,1,&unused)==-1&&errno==EPROTO);
                assert(unused==-1);
            }
        } else if(which==1)assert(mulib_wayland_wire_stream_peek(&stream,&f)==-1&&errno==EPROTO);
        assert(stream.failed&&stream.fd_n==0);
        mulib_wayland_wire_stream_free(&stream);
        assert(close(fd[1])==0&&close(pipefd[0])==0&&close(pipefd[1])==0);
        assert(open_fds()==baseline);
    }
}
static void test_wayland_send_failure(void) {
    size_t baseline=open_fds();
    int fd[2],pipefd[2];
    pair_open(fd); assert(pipe(pipefd)==0);
    mway_tx tx={0};
    assert(mulib_wayland_wire_tx_begin(&tx,1,0,NULL,0,pipefd[0]));
    assert(close(fd[1])==0);
    assert(mulib_wayland_wire_tx_step(&tx,fd[0])==-1);
    assert(tx.failed&&!tx.has_fd);
    assert(fcntl(pipefd[0],F_GETFD)>=0); /* Borrowed source not closed. */
    mulib_wayland_wire_tx_free(&tx);
    assert(close(fd[0])==0&&close(pipefd[0])==0&&close(pipefd[1])==0);
    assert(open_fds()==baseline);
}
static void test_wayland_shm_and_text(void) {
    size_t required=0;
    assert(mulib_wayland_shm_buffer_extent(4,2,3,12,0,&required)&&required==40);
    assert(!mulib_wayland_shm_buffer_extent(0,2,3,7,0,&required));
    assert(!mulib_wayland_shm_buffer_extent(0,2,3,8,2,&required));
    assert(!mulib_wayland_shm_buffer_extent(INT_MAX,1,1,4,0,&required)&&errno==EOVERFLOW);
    assert(!mulib_wayland_shm_buffer_extent(0,1,INT_MAX,INT_MAX-3,0,&required));
    int fd[2]; pair_open(fd);
    assert(!mulib_wayland_shm_buffer_bounded(fd[0],2,3,4,2,3,12,0,39));
    assert_empty(fd[1]);
    assert(mulib_wayland_shm_buffer_bounded(fd[0],2,3,4,2,3,12,0,40));
    unsigned char msg[32]; read_exact(fd[1],msg,sizeof msg);
    mway_frame f;
    assert(mulib_wayland_wire_parse(msg,sizeof msg,&f)==1&&f.object==2&&f.n==24);
    assert(!mulib_wayland_shm_pool(fd[0],1,2,-1,4096));
    assert(!mulib_wayland_data_receive(fd[0],1,"text/plain",-1));
    assert(!mulib_wayland_dmabuf_plane(fd[0],1,-1,0,0,8,0));
    assert(!mulib_wayland_xdg_title(fd[0],1,NULL));
    assert(mulib_wayland_textinput_text_valid("a\xc3\xa9",3,1,3));
    assert(!mulib_wayland_textinput_text_valid("a\xc3\xa9",3,2,3));
    assert(!mulib_wayland_textinput_text_valid("\xc0\x80",2,0,2));
    assert(!mulib_wayland_textinput_text_valid("\xed\xa0\x80",3,0,3));
    assert(!mulib_wayland_textinput_text_valid("\xf4\x90\x80\x80",4,0,4));
    assert(!mulib_wayland_textinput_text_valid("\xe2\x82",2,0,2));
    char too_long[4002]; memset(too_long,'a',sizeof too_long); too_long[4001]=0;
    assert(!mulib_wayland_textinput_surrounding(fd[0],2,too_long,0,0));
    assert_empty(fd[1]);
    pair_close(fd);
}
static void test_wayland_backend(void) {
    int fd[2]; pair_open(fd);
    uint32_t source[]={0x80ff8040,0xffffffff,0xfeedface,0,0xff123456,0xfeedface};
    uint32_t pixels[2][6]={{0},{0}};
    mgx_surface s={.pixels=source,.stride=3,.w=2,.h=2};
    mulib_glx_backend_wayland_t b={.fd=fd[0],.surface=10,.buffer={20,21},.configured=1};
    for(int i=0;i<2;i++)b.maps[i]=(mgx_surface){.pixels=pixels[i],.stride=3,.w=2,.h=2};
    mgx_rect damage[]={ {-1,-1,3,3},{INT_MAX,INT_MAX,INT_MAX,INT_MAX} };
    assert(mulib_glx_backend_wayland_begin(&b,&s,damage,2));
    assert(b.busy[0]&&!b.busy[1]&&pixels[0][0]==0x80804020);
    assert(pixels[0][2]==0&&pixels[0][5]==0); /* Padding is untouched. */
    uint32_t first=pixels[0][0];
    source[0]=0xff000000;
    assert(!mulib_glx_backend_wayland_begin(&b,&s,NULL,0)&&pixels[0][0]==first);
    mulib_glx_backend_wayland_release(&b,20); /* Not a committed/releasable buffer yet. */
    assert(b.busy[0]);
    assert(mulib_glx_backend_wayland_flush(&b)==1&&b.busy[0]&&!b.tx.bytes.v);
    unsigned char bytes[52]; read_exact(fd[1],bytes,sizeof bytes);
    mway_frame f;
    assert(mulib_wayland_wire_parse(bytes,52,&f)==1&&f.opcode==1&&f.consumed==20);
    assert(mulib_wayland_wire_parse(bytes+20,32,&f)==1&&f.opcode==2&&f.n==16);
    uint32_t rect[4]; memcpy(rect,f.args,sizeof rect);
    assert(rect[0]==0&&rect[1]==0&&rect[2]==2&&rect[3]==2);
    assert(mulib_wayland_wire_parse(bytes+44,8,&f)==1&&f.opcode==6);
    assert(mulib_glx_backend_wayland_present(&b,&s,NULL,0));
    read_exact(fd[1],bytes,sizeof bytes);
    assert(b.busy[0]&&b.busy[1]&&pixels[0][0]==first&&pixels[1][0]==source[0]);
    assert(!mulib_glx_backend_wayland_begin(&b,&s,NULL,0)&&errno==EAGAIN);
    mulib_glx_backend_wayland_release(&b,0);
    mulib_glx_backend_wayland_release(&b,999);
    assert(b.busy[0]&&b.busy[1]);
    mulib_glx_backend_wayland_release(&b,20);
    assert(!b.busy[0]&&b.busy[1]);
    mgx_rect invalid[]={{0,0,1,1},{0,0,-1,1}};
    assert(!mulib_glx_backend_wayland_begin(&b,&s,invalid,2)&&pixels[0][0]==first);
    s.stride=1;
    assert(!mulib_glx_backend_wayland_begin(&b,&s,NULL,0));
    s.stride=SIZE_MAX;
    assert(!mulib_glx_backend_wayland_begin(&b,&s,NULL,0));
    s.stride=3;
    uint32_t *saved=b.maps[0].pixels;
    b.maps[0].pixels=source;
    assert(!mulib_glx_backend_wayland_begin(&b,&s,NULL,0));
    b.maps[0].pixels=saved;
    assert_empty(fd[1]);
    assert(mulib_glx_backend_wayland_begin(&b,&s,NULL,0));
    assert(close(fd[1])==0);
    assert(mulib_glx_backend_wayland_flush(&b)==-1);
    assert(b.failed&&!b.configured&&b.busy[0]&&!b.tx.bytes.v);
    assert(!mulib_glx_backend_wayland_present(&b,&s,NULL,0));
    mulib_glx_backend_wayland_cancel(&b);
    assert(close(fd[0])==0);
}
static void make_setup(unsigned char body[112]) {
    memset(body,0,112);
    mx11_put32(body+4,0x10000000); mx11_put32(body+8,0x0000ffff);
    mx11_put16(body+18,65535);
    body[20]=1; body[21]=1; body[22]=0;
    body[32]=24; body[33]=32; body[34]=32; /* Pixmap format. */
    body[40+38]=24; body[40+39]=1; /* Screen root depth, depth count. */
    body[80]=24; mx11_put16(body+82,1); /* One visual. */
    body[88+4]=4; /* TrueColor, though visual selection is caller-owned. */
}
static void test_x11_setup_and_ids(void) {
    unsigned char body[112]; make_setup(body);
    assert(mulib_x11_wire_setup_body_valid(body,sizeof body));
    assert(mulib_x11_image_setup_format(body,sizeof body,24));
    assert(!mulib_x11_image_setup_format(body,sizeof body,16));
    body[22]=1; assert(!mulib_x11_image_setup_format(body,sizeof body,24)); body[22]=0;
    for(size_t n=0;n<sizeof body;n++)assert(!mulib_x11_wire_setup_body_valid(body,n));
    mx11_put16(body+82,65535);
    assert(!mulib_x11_wire_setup_body_valid(body,sizeof body));
    make_setup(body); mx11_put16(body+16,65535);
    assert(!mulib_x11_wire_setup_body_valid(body,sizeof body));
    make_setup(body);
    int fd[2]; pair_open(fd);
    unsigned char pre[8]={1,0,11,0,0,0,28,0};
    assert(send(fd[1],pre,8,MSG_NOSIGNAL)==8);
    assert(send(fd[1],body,sizeof body,MSG_NOSIGNAL)==(ssize_t)sizeof body);
    mx11_connection c={0};
    unsigned char *reply=NULL;
    size_t reply_n=0;
    const unsigned char cookie[]={1,2,3};
    assert(mulib_x11_wire_setup(&c,fd[0],cookie,sizeof cookie,&reply,&reply_n));
    assert(reply_n==112&&!memcmp(reply,body,112));
    assert(c.fd==fd[0]&&c.resource_base==0x10000000&&c.resource_mask==65535);
    unsigned char request[36]; read_exact(fd[1],request,sizeof request);
    assert(request[0]=='l'&&mx11_get16(request+2)==11&&mx11_get16(request+6)==18);
    assert(mx11_get16(request+8)==3&&!memcmp(request+12,"MIT-MAGIC-COOKIE-1",18));
    assert(!memcmp(request+32,cookie,3)&&request[35]==0);
    free(reply); pair_close(fd);
    c=(mx11_connection){.resource_base=0x10000000,.resource_mask=5};
    assert(mulib_x11_wire_id(&c)==0x10000001);
    assert(mulib_x11_wire_id(&c)==0x10000004);
    assert(mulib_x11_wire_id(&c)==0x10000005);
    for(unsigned i=0;i<8;i++)assert(!mulib_x11_wire_id(&c)&&c.next==3);
    c=(mx11_connection){.resource_mask=UINT32_MAX,.next=UINT32_MAX-1};
    assert(mulib_x11_wire_id(&c)==UINT32_MAX&&c.next==UINT32_MAX);
    assert(!mulib_x11_wire_id(&c)&&c.next==UINT32_MAX);
}
static void test_x11_setup_failures(void) {
    for(int which=0;which<3;which++) {
        int fd[2]; pair_open(fd);
        unsigned char body[112]; make_setup(body);
        unsigned char pre[8]={1,0,11,0,0,0,28,0};
        if(which==0)pre[0]=0; /* Authentication failure is not setup success. */
        if(which==1)body[79]=255; /* Declared depth array exceeds body. */
        assert(send(fd[1],pre,8,MSG_NOSIGNAL)==8);
        size_t n=which==2?12:sizeof body;
        assert(send(fd[1],body,n,MSG_NOSIGNAL)==(ssize_t)n);
        if(which==2)assert(shutdown(fd[1],SHUT_WR)==0);
        mx11_connection c={.fd=123};
        unsigned char *reply=(unsigned char *)&c;
        size_t reply_n=123;
        assert(!mulib_x11_wire_setup(&c,fd[0],NULL,0,&reply,&reply_n));
        assert(c.fd==123&&reply==NULL&&reply_n==0);
        assert(errno==(which==2?ECONNRESET:EPROTO));
        unsigned char request[12]; read_exact(fd[1],request,sizeof request);
        pair_close(fd);
    }
}
static void test_x11_partial_request(void) {
    int fd[2]; pair_open(fd);
    mx11_connection c={.fd=fd[0],.max_request=65535};
    size_t n=262136;
    unsigned char *data=(unsigned char *)malloc(n);
    assert(data);
    memset(data,0x35,n);
    mx11_request_tx tx={0};
    assert(mulib_x11_wire_request_begin(&tx,&c,127,3,data,n));
    memset(data,0,n); /* begin owns a snapshot. */
    int r=mulib_x11_wire_request_step(&tx);
    assert(r==0&&tx.offset>0&&tx.offset<tx.n&&c.sequence==0);
    mway_bytes got={0};
    for(unsigned turns=0;;turns++) {
        assert(turns<10000);
        unsigned char bytes[8192];
        ssize_t k=recv(fd[1],bytes,sizeof bytes,MSG_DONTWAIT);
        if(k<0)assert(errno==EAGAIN||errno==EWOULDBLOCK);
        else assert(k>0&&mway_append(&got,bytes,(size_t)k));
        if(r!=1)r=mulib_x11_wire_request_step(&tx);
        assert(r>=0);
        if(r==1&&got.n==tx.n)break;
    }
    assert(c.sequence==1&&got.n==n+4&&!memcmp(got.v,tx.data,got.n));
    assert(got.v[4]==0x35&&got.v[n+3]==0x35&&mx11_get16(got.v+2)==65535);
    assert(mulib_x11_wire_request_step(&tx)==1&&c.sequence==1);
    mulib_x11_wire_request_free(&tx);
    assert(!c.failed);
    assert(!mulib_x11_wire_request_begin(&tx,&c,1,0,NULL,4));
    assert(!mulib_x11_wire_request_begin(&tx,&c,1,0,data,262140));
    assert_empty(fd[1]);
    free(data); mway_bytes_free(&got); pair_close(fd);
}
static void test_x11_partial_cancel_and_read(void) {
    int fd[2]; pair_open(fd);
    mx11_connection c={.fd=fd[0],.max_request=65535};
    unsigned char *data=(unsigned char *)calloc(65536,1);
    assert(data);
    mx11_request_tx tx={0};
    assert(mulib_x11_wire_request_begin(&tx,&c,1,0,data,65536));
    assert(mulib_x11_wire_request_step(&tx)==0&&tx.offset>0);
    mulib_x11_wire_request_free(&tx);
    assert(c.failed&&!c.sequence);
    assert(!mulib_x11_wire_request(&c,1,0,NULL,0));
    free(data); pair_close(fd);
    pair_open(fd);
    unsigned char buf[8]={0};
    size_t offset=0;
    assert(mx11_read_step(fd[1],buf,8,&offset)==0&&offset==0);
    assert(send(fd[0],"abc",3,MSG_NOSIGNAL)==3);
    assert(mx11_read_step(fd[1],buf,8,&offset)==0&&offset==3);
    assert(send(fd[0],"defgh",5,MSG_NOSIGNAL)==5);
    assert(mx11_read_step(fd[1],buf,8,&offset)==1&&offset==8&&!memcmp(buf,"abcdefgh",8));
    offset=0;
    assert(close(fd[0])==0);
    assert(mx11_read_step(fd[1],buf,8,&offset)==-1&&errno==ECONNRESET);
    assert(close(fd[1])==0);
    pair_open(fd);
    c=(mx11_connection){.fd=fd[0],.max_request=32};
    assert(close(fd[1])==0);
    assert(!mulib_x11_wire_request(&c,1,0,NULL,0)&&c.failed&&!c.sequence);
    assert(close(fd[0])==0);
}
static void test_x11_frames_and_events(void) {
    unsigned char p[64]={0};
    mx11_frame f={0};
    p[0]=1; mx11_put16(p+2,7); mx11_put32(p+4,2);
    for(size_t n=0;n<40;n++)assert(mulib_x11_wire_parse(p,n,0,&f)==0);
    assert(mulib_x11_wire_parse(p,64,0,&f)==1&&f.consumed==40&&f.sequence==7);
    p[0]=35|128;
    assert(mulib_x11_wire_parse(p,40,0,&f)==1&&f.type==35&&f.consumed==40);
    mx11_put32(p+4,UINT32_MAX);
    assert(mulib_x11_wire_parse(p,64,0,&f)==-1&&errno==EMSGSIZE);
    mx11_put32(p+4,1);
    assert(mulib_x11_wire_parse(p,64,32,&f)==-1&&errno==EMSGSIZE);
    p[0]=0;
    assert(mulib_x11_wire_parse(p,32,32,&f)==1&&f.type==0&&f.consumed==32);
    assert(mulib_x11_wire_parse(NULL,32,0,&f)==-1);
    memset(p,0,sizeof p); p[0]=2|128; p[1]=37;
    mx11_put16(p+24,(uint16_t)-12); mx11_put16(p+26,9);
    mgx_event e={.code=999};
    assert(mulib_x11_events_decode_bounded(p,31,&e)==-1&&e.code==999);
    assert(mulib_x11_events_decode_bounded(p,32,&e)==1);
    assert(e.type==MGX_EVENT_KEY&&e.code==37&&e.down==1&&e.x==-12&&e.y==9);
    assert(mulib_x11_events_decode(p,&e)&&e.code==29); /* Legacy contract. */
    p[0]=5; p[1]=4;
    e.code=999;
    assert(mulib_x11_events_decode_bounded(p,32,&e)==0&&e.code==999);
    p[0]=4;
    assert(mulib_x11_events_decode_bounded(p,32,&e)==1&&e.type==MGX_EVENT_SCROLL&&e.dy==1);
    p[1]=0;
    assert(mulib_x11_events_decode_bounded(p,32,&e)==-1&&errno==EPROTO);
}
static void test_x11_image(void) {
    int fd[2]; pair_open(fd);
    mx11_connection c={.fd=fd[0],.max_request=10};
    uint32_t pixels[]={0xff010203,0xff040506,0xdeadbeef,
                       0xff070809,0xff0a0b0c,0xdeadbeef,
                       0xff0d0e0f,0xff101112,0xdeadbeef};
    mgx_surface s={.pixels=pixels,.stride=3,.w=2,.h=3};
    assert(!mulib_x11_image_put(NULL,1,2,&s,24));
    s.stride=1; assert(!mulib_x11_image_put(&c,1,2,&s,24));
    s.stride=SIZE_MAX; assert(!mulib_x11_image_put(&c,1,2,&s,24));
    s.stride=3; assert(!mulib_x11_image_put(&c,1,2,&s,16));
    assert_empty(fd[1]);
    mulib_glx_backend_x11_t b={.connection=&c,.window=1,.gc=2,.depth=24};
    assert(mulib_glx_backend_x11_present(&b,&s,NULL,0)&&c.sequence==2);
    unsigned char wire[72]; read_exact(fd[1],wire,sizeof wire);
    assert(wire[0]==72&&wire[1]==2&&mx11_get16(wire+2)==10);
    assert(mx11_get16(wire+12)==2&&mx11_get16(wire+14)==2);
    assert(mx11_get32(wire+24)==pixels[0]&&mx11_get32(wire+28)==pixels[1]);
    assert(mx11_get32(wire+32)==pixels[3]&&mx11_get32(wire+36)==pixels[4]);
    assert(wire[40]==72&&mx11_get16(wire+42)==8&&mx11_get16(wire+58)==2);
    assert(mx11_get32(wire+64)==pixels[6]&&mx11_get32(wire+68)==pixels[7]);
    pair_close(fd);
}
static void test_serializer_rejections(void) {
    int fd[2]; pair_open(fd);
    mx11_connection c={.fd=fd[0],.max_request=10};
    assert(!mulib_x11_window_map(&c,0));
    assert(!mulib_x11_window_destroy(&c,0));
    assert(!mulib_x11_window_gc(&c,0,1));
    assert(!mulib_x11_window_gc(&c,1,0));
    assert(!mulib_x11_selection_owner(&c,1,0,0));
    assert(!mulib_x11_selection_convert(&c,1,1,0,0,0));
    assert(!mulib_x11_wm_property(&c,1,1,1,8,"12345678901234567",17)&&errno==EMSGSIZE);
    assert(!mulib_x11_wm_property(&c,1,1,1,16,"a",1));
    assert(!mulib_x11_wm_property(&c,0,1,1,8,"a",1));
    assert(!mulib_wayland_xdg_surface(fd[0],1,0,1));
    assert(!mulib_wayland_xdg_toplevel(fd[0],1,0));
    assert(!mulib_wayland_seat_pointer(fd[0],1,0));
    assert(!mulib_wayland_seat_keyboard(fd[0],1,0));
    assert(!mulib_wayland_seat_touch(fd[0],1,0));
    assert(!mulib_wayland_dmabuf_params(fd[0],1,0));
    assert(!mulib_wayland_dmabuf_create_immediate(fd[0],1,2,1,1,0,8));
    assert(!mulib_wayland_protocol_bind(fd[0],1,1,NULL,1,2));
    assert(!mulib_wayland_protocol_registry(fd[0],0));
    assert(!mulib_wayland_data_offer(fd[0],1,NULL));
    assert_empty(fd[1]);
    /* Empty selection owner is valid (clear); property None on convert is valid. */
    assert(mulib_x11_selection_owner(&c,0,1,0));
    unsigned char request[16]; read_exact(fd[1],request,sizeof request);
    assert(request[0]==22&&mx11_get32(request+4)==0);
    pair_close(fd);
}
static void test_synchronous_nonblocking_compatibility(void) {
    for(int wayland=0;wayland<2;wayland++) {
        int fd[2]; pair_open(fd);
        size_t n=wayland?65524:262136;
        unsigned char *data=(unsigned char *)malloc(n);
        assert(data); memset(data,0x71,n);
        pid_t child=fork();
        assert(child>=0);
        if(child==0) {
            assert(close(fd[0])==0);
            size_t total=n+(wayland?8:4);
            unsigned char *got=(unsigned char *)malloc(total);
            assert(got&&mx11_read(fd[1],got,total));
            size_t header=wayland?8:4;
            for(size_t i=header;i<total;i++)assert(got[i]==0x71);
            free(got); free(data);
            assert(close(fd[1])==0);
            _exit(0);
        }
        assert(close(fd[1])==0);
        if(wayland)assert(mulib_wayland_wire_send(fd[0],1,0,data,n,-1)==0);
        else {
            mx11_connection c={.fd=fd[0],.max_request=65535};
            assert(mulib_x11_wire_request(&c,127,0,data,n)&&c.sequence==1);
        }
        int status=0;
        assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
        assert(close(fd[0])==0);
        free(data);
    }
}
int main(void) {
    size_t baseline=open_fds();
    test_wayland_frames();
    test_wayland_partial_send();
    test_wayland_receive_ownership();
    test_wayland_stream();
    test_wayland_stream_errors();
    test_wayland_send_failure();
    test_wayland_shm_and_text();
    test_wayland_backend();
    test_x11_setup_and_ids();
    test_x11_setup_failures();
    test_x11_partial_request();
    test_x11_partial_cancel_and_read();
    test_x11_frames_and_events();
    test_x11_image();
    test_serializer_rejections();
    test_synchronous_nonblocking_compatibility();
    assert(open_fds()==baseline);
    puts("gfx-wire: socketpair transport/backend tests passed");
    return 0;
}
