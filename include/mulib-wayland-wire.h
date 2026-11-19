/* MuLIB graphical extension: bounded native Wayland transport. C11. */
#ifndef MULIB_WAYLAND_WIRE_H
#define MULIB_WAYLAND_WIRE_H
#include "mulib-glx-core.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

typedef struct { unsigned char *v; size_t n,cap; } mway_bytes;
static inline int mway_append(mway_bytes *b,const void *p,size_t n) {
    if(!b||(!p&&n)||n>SIZE_MAX-b->n) { errno=EINVAL; return 0; }
    if(!mgx_grow((void **)&b->v,&b->cap,b->n+n,1)) { errno=ENOMEM; return 0; }
    if(n)memcpy(b->v+b->n,p,n);
    b->n+=n;
    return 1;
}
static inline int mway_u32(mway_bytes *b,uint32_t n) { return mway_append(b,&n,4); }
/* Allocation failure leaves the original message length unchanged. */
static inline int mway_string(mway_bytes *b,const char *s) {
    uint32_t zero=0;
    if(!b) { errno=EINVAL; return 0; }
    if(!s)return mway_u32(b,0);
    size_t n=strlen(s);
    if(n>=65520) { errno=EMSGSIZE; return 0; }
    n++;
    size_t old=b->n;
    if(mway_u32(b,(uint32_t)n)&&mway_append(b,s,n)&&
       mway_append(b,&zero,(4-n%4)%4))return 1;
    b->n=old;
    return 0;
}
static inline void mway_bytes_free(mway_bytes *b) {
    if(b) { free(b->v); memset(b,0,sizeof *b); }
}
/* Append one complete request, including its native-endian wire header. */
static inline int mway_message(mway_bytes *b,uint32_t object,uint16_t opcode,
                               const void *args,size_t n) {
    if(!b||!object||n%4||n>65524||(!args&&n)) { errno=EINVAL; return 0; }
    size_t old=b->n;
    if(mway_u32(b,object)&&mway_u32(b,((uint32_t)n+8)<<16|opcode)&&
       mway_append(b,args,n))return 1;
    b->n=old;
    return 0;
}
static inline int mway_cloexec(int fd) {
    int flags=fcntl(fd,F_GETFD);
    return flags<0?-1:fcntl(fd,F_SETFD,flags|FD_CLOEXEC);
}
static inline int mway_dup_cloexec(int fd) {
    int copy;
#ifdef F_DUPFD_CLOEXEC
    copy=fcntl(fd,F_DUPFD_CLOEXEC,0);
    if(copy>=0||errno!=EINVAL)return copy;
#endif
    copy=dup(fd);
    if(copy>=0&&mway_cloexec(copy)<0) {
        int e=errno; close(copy); errno=e; return -1;
    }
    return copy;
}
static inline int mway_wait(int fd,short events) {
    struct pollfd p={fd,events,0};
    int r;
    do { r=poll(&p,1,-1); } while(r<0&&errno==EINTR);
    if(r>0&&(p.revents&POLLNVAL)) { errno=EBADF; return -1; }
    return r>0?0:-1;
}
static inline int mulib_wayland_wire_connect(const char *path) {
    struct sockaddr_un addr={0};
    if(!path||!path[0]||strlen(path)>=sizeof addr.sun_path) { errno=EINVAL; return -1; }
    int fd;
#ifdef SOCK_CLOEXEC
    fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    if(fd<0&&errno==EINVAL)fd=socket(AF_UNIX,SOCK_STREAM,0);
#else
    fd=socket(AF_UNIX,SOCK_STREAM,0);
#endif
    if(fd<0)return -1;
    addr.sun_family=AF_UNIX;
    strcpy(addr.sun_path,path);
    if(mway_cloexec(fd)<0||connect(fd,(struct sockaddr *)&addr,
          (socklen_t)(offsetof(struct sockaddr_un,sun_path)+strlen(path)+1))<0) {
        int e=errno; close(fd); errno=e; return -1;
    }
    return fd;
}

/* Zero-init; owns bytes and a duplicate of sendfd, never the caller's FD.
 * step: 1 complete, 0 EAGAIN, -1 fatal. A fatal partial send poisons the
 * connection: discard that connection rather than retrying the request. */
typedef struct {
    mway_bytes bytes;
    size_t sent;
    int sendfd,has_fd,failed;
} mway_tx;
typedef mway_tx mulib_wayland_wire_tx_t;
static inline void mulib_wayland_wire_tx_free(mway_tx *t) {
    if(!t)return;
    if(t->has_fd)close(t->sendfd);
    mway_bytes_free(&t->bytes);
    memset(t,0,sizeof *t);
}
static inline int mulib_wayland_wire_tx_begin(mway_tx *t,uint32_t object,
        uint16_t opcode,const void *args,size_t n,int sendfd) {
    if(!t||t->bytes.v||t->has_fd||t->failed||sendfd< -1) { errno=EINVAL; return 0; }
    if(!mway_message(&t->bytes,object,opcode,args,n)) {
        int e=errno; mulib_wayland_wire_tx_free(t); errno=e; return 0;
    }
    if(sendfd>=0) {
        t->sendfd=mway_dup_cloexec(sendfd);
        if(t->sendfd<0) { int e=errno; mulib_wayland_wire_tx_free(t); errno=e; return 0; }
        t->has_fd=1;
    }
    return 1;
}
static inline int mulib_wayland_wire_tx_step(mway_tx *t,int fd) {
    if(!t||t->failed||!t->bytes.n||t->sent>t->bytes.n) { errno=EINVAL; return -1; }
    while(t->sent<t->bytes.n) {
        struct iovec iov={t->bytes.v+t->sent,t->bytes.n-t->sent};
        struct msghdr msg={0};
        union { struct cmsghdr align; char b[CMSG_SPACE(sizeof(int))]; } control;
        msg.msg_iov=&iov; msg.msg_iovlen=1;
        if(t->has_fd) {
            memset(&control,0,sizeof control);
            msg.msg_control=control.b; msg.msg_controllen=sizeof control.b;
            struct cmsghdr *c=CMSG_FIRSTHDR(&msg);
            c->cmsg_level=SOL_SOCKET; c->cmsg_type=SCM_RIGHTS;
            c->cmsg_len=CMSG_LEN(sizeof(int));
            memcpy(CMSG_DATA(c),&t->sendfd,sizeof(int));
        }
        ssize_t k=sendmsg(fd,&msg,MSG_NOSIGNAL|MSG_DONTWAIT);
        if(k<0&&errno==EINTR)continue;
        if(k<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;
        if(k<=0) {
            int e=k?errno:EPIPE;
            if(t->has_fd) { close(t->sendfd); t->has_fd=0; }
            t->failed=1; errno=e; return -1;
        }
        /* SCM_RIGHTS is delivered with the first successful byte, only once. */
        if(t->has_fd) { close(t->sendfd); t->has_fd=0; }
        t->sent+=(size_t)k;
    }
    return 1;
}
/* Compatibility API: completes synchronously, even for O_NONBLOCK sockets.
 * Event loops should use tx_begin/tx_step instead. */
static inline int mulib_wayland_wire_send(int fd,uint32_t object,uint16_t opcode,
        const void *args,size_t n,int sendfd) {
    mway_tx t={0};
    if(!mulib_wayland_wire_tx_begin(&t,object,opcode,args,n,sendfd))return -1;
    int r;
    while((r=mulib_wayland_wire_tx_step(&t,fd))==0) {
        if(mway_wait(fd,POLLOUT)<0) { r=-1; break; }
    }
    int e=errno;
    mulib_wayland_wire_tx_free(&t);
    errno=e;
    return r==1?0:-1;
}

typedef struct {
    uint32_t object;
    uint16_t opcode;
    const unsigned char *args;
    size_t n,consumed;
} mway_frame;
typedef mway_frame mulib_wayland_wire_frame_t;
static inline int mulib_wayland_wire_parse(const void *p,size_t n,mway_frame *out) {
    if(!out||(!p&&n)) { errno=EINVAL; return -1; }
    if(n<8)return 0;
    uint32_t a,b;
    memcpy(&a,p,4); memcpy(&b,(const unsigned char *)p+4,4);
    size_t z=b>>16;
    if(!a||z<8||z%4) { errno=EPROTO; return -1; }
    if(z>n)return 0;
    *out=(mway_frame){a,(uint16_t)b,(const unsigned char *)p+8,z-8,z};
    return 1;
}
/* Successful receive transfers all FDs to the caller. Every FD obtained by a
 * failed/truncated receive is closed; the bytes from that receive are unusable.
 * cap must be nonzero so SCM_RIGHTS cannot be consumed without stream bytes. */
static inline ssize_t mulib_wayland_wire_receive_flags(int fd,void *buf,size_t cap,
        int *fds,size_t fd_cap,size_t *fd_n,int flags) {
    if(fd_n)*fd_n=0;
    if(!fd_n||!buf||!cap||(!fds&&fd_cap)) { errno=EINVAL; return -1; }
    struct iovec iov={buf,cap};
    union { struct cmsghdr align; char b[CMSG_SPACE(32*sizeof(int))]; } control;
    struct msghdr m={0};
    m.msg_iov=&iov; m.msg_iovlen=1;
    m.msg_control=control.b; m.msg_controllen=sizeof control.b;
#ifdef MSG_CMSG_CLOEXEC
    flags|=MSG_CMSG_CLOEXEC;
#endif
    ssize_t n;
    do { n=recvmsg(fd,&m,flags); } while(n<0&&errno==EINTR);
    if(n<0)return n;
    int bad=(m.msg_flags&(MSG_CTRUNC|MSG_TRUNC))!=0;
    int error=EMSGSIZE;
    for(struct cmsghdr *c=CMSG_FIRSTHDR(&m);c;c=CMSG_NXTHDR(&m,c)) {
        if(c->cmsg_level!=SOL_SOCKET||c->cmsg_type!=SCM_RIGHTS)continue;
        if(c->cmsg_len<CMSG_LEN(0)) { bad=1; continue; }
        size_t bytes=c->cmsg_len-CMSG_LEN(0);
        if(bytes%sizeof(int))bad=1;
        for(size_t i=0;i<bytes/sizeof(int);i++) {
            int f;
            memcpy(&f,(unsigned char *)CMSG_DATA(c)+i*sizeof(int),sizeof f);
            if(*fd_n>=fd_cap) { close(f); bad=1; }
            else if(mway_cloexec(f)<0) { error=errno; close(f); bad=1; }
            else fds[(*fd_n)++]=f;
        }
    }
    if(bad) {
        for(size_t i=0;i<*fd_n;i++)close(fds[i]);
        *fd_n=0; errno=error; return -1;
    }
    return n;
}
static inline ssize_t mulib_wayland_wire_receive(int fd,void *buf,size_t cap,
        int *fds,size_t fd_cap,size_t *fd_n) {
    return mulib_wayland_wire_receive_flags(fd,buf,cap,fds,fd_cap,fd_n,0);
}

/* Bounded nonblocking receive accumulator. FDs are a separate ordered queue:
 * protocol signatures, NOT recvmsg byte boundaries, determine consumption.
 * Default byte cap 256 KiB, FD cap 32. Do not copy this owning object. */
typedef struct {
    mway_bytes bytes;
    int fds[32];
    size_t fd_n,max_bytes;
    int eof,failed;
} mway_stream;
typedef mway_stream mulib_wayland_wire_stream_t;
static inline void mulib_wayland_wire_stream_free(mway_stream *s) {
    if(!s)return;
    for(size_t i=0;i<s->fd_n;i++)close(s->fds[i]);
    mway_bytes_free(&s->bytes);
    memset(s,0,sizeof *s);
}
static inline int mway_stream_fail(mway_stream *s,int error) {
    for(size_t i=0;i<s->fd_n;i++)close(s->fds[i]);
    s->fd_n=0; s->bytes.n=0; s->failed=1; errno=error;
    return -1;
}
static inline ssize_t mulib_wayland_wire_stream_receive(mway_stream *s,int fd) {
    if(!s||s->failed) { errno=EINVAL; return -1; }
    if(s->eof)return 0;
    size_t limit=s->max_bytes?s->max_bytes:262144;
    if(s->bytes.n>=limit)return mway_stream_fail(s,EMSGSIZE);
    size_t room=limit-s->bytes.n;
    if(room>4096)room=4096;
    if(!mgx_grow((void **)&s->bytes.v,&s->bytes.cap,s->bytes.n+room,1))
        return mway_stream_fail(s,ENOMEM);
    size_t got=0;
    ssize_t n=mulib_wayland_wire_receive_flags(fd,s->bytes.v+s->bytes.n,room,
                  s->fds+s->fd_n,32-s->fd_n,&got,MSG_DONTWAIT);
    if(n<0) {
        if(errno==EAGAIN||errno==EWOULDBLOCK)return -1;
        return mway_stream_fail(s,errno);
    }
    s->fd_n+=got;
    s->bytes.n+=(size_t)n;
    if(!n)s->eof=1;
    return n;
}
/* frame args are borrowed until the next receive/consume/free. */
static inline int mulib_wayland_wire_stream_peek(mway_stream *s,mway_frame *f) {
    if(!s||!f||s->failed) { errno=EINVAL; return -1; }
    int r=mulib_wayland_wire_parse(s->bytes.v,s->bytes.n,f);
    if(r<0)return mway_stream_fail(s,errno);
    if(!r&&s->bytes.n>=8) {
        uint32_t header;
        memcpy(&header,s->bytes.v+4,4);
        size_t limit=s->max_bytes?s->max_bytes:262144;
        if((size_t)(header>>16)>limit)return mway_stream_fail(s,EMSGSIZE);
    }
    if(!r&&s->eof&&(s->bytes.n||s->fd_n))return mway_stream_fail(s,EPROTO);
    return r;
}
/* Copy/decode borrowed frame arguments before consuming. Caller supplies its
 * signature's FD count; ownership transfers only on return 1. Dispatch FD-using
 * callbacks after that transfer, with copied arguments, not stale frame views. */
static inline int mulib_wayland_wire_stream_consume(mway_stream *s,size_t fd_count,int *fds) {
    if(fd_count>32||(!fds&&fd_count)) { errno=EINVAL; return -1; }
    mway_frame f;
    int r=mulib_wayland_wire_stream_peek(s,&f);
    if(r!=1)return r;
    if(fd_count>s->fd_n) {
        if(s->eof)return mway_stream_fail(s,EPROTO);
        return 0;
    }
    if(fd_count)memcpy(fds,s->fds,fd_count*sizeof(int));
    s->fd_n-=fd_count;
    memmove(s->fds,s->fds+fd_count,s->fd_n*sizeof(int));
    s->bytes.n-=f.consumed;
    memmove(s->bytes.v,s->bytes.v+f.consumed,s->bytes.n);
    return 1;
}
#endif
