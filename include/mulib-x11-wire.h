/* MuLIB graphical extension: bounded little-endian X11 transport. C11. */
#ifndef MULIB_X11_WIRE_H
#define MULIB_X11_WIRE_H
#include "mulib-glx-core.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <poll.h>

typedef struct {
    int fd;
    uint16_t sequence;
    uint32_t resource_base,resource_mask,next;
    uint16_t max_request;
    int failed; /* Fatal/cancelled partial request: close, do not reuse. */
} mx11_connection;
typedef mx11_connection mulib_x11_wire_connection_t;
static inline void mx11_put16(unsigned char *p,uint16_t x) {
    p[0]=(unsigned char)x; p[1]=(unsigned char)(x>>8);
}
static inline void mx11_put32(unsigned char *p,uint32_t x) {
    for(int i=0;i<4;i++)p[i]=(unsigned char)(x>>(i*8));
}
static inline uint16_t mx11_get16(const unsigned char *p) {
    return (uint16_t)(p[0]|p[1]<<8);
}
static inline uint32_t mx11_get32(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static inline int mx11_wait(int fd,short events) {
    struct pollfd p={fd,events,0};
    int r;
    do { r=poll(&p,1,-1); } while(r<0&&errno==EINTR);
    if(r>0&&(p.revents&POLLNVAL)) { errno=EBADF; return 0; }
    return r>0;
}
/* Resumable borrowed-buffer cursors. 1 done, 0 would block, -1 fatal.
 * Do not change buffers or interleave operations until their cursor completes. */
static inline int mx11_write_step(int fd,const void *p,size_t n,size_t *offset) {
    if(!offset||*offset>n||(!p&&n)) { errno=EINVAL; return -1; }
    while(*offset<n) {
        ssize_t k=send(fd,(const unsigned char *)p+*offset,n-*offset,MSG_NOSIGNAL|MSG_DONTWAIT);
        if(k<0&&errno==EINTR)continue;
        if(k<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;
        if(k<=0) { if(!k)errno=EPIPE; return -1; }
        *offset+=(size_t)k;
    }
    return 1;
}
static inline int mx11_read_step(int fd,void *p,size_t n,size_t *offset) {
    if(!offset||*offset>n||(!p&&n)) { errno=EINVAL; return -1; }
    while(*offset<n) {
        ssize_t k=recv(fd,(unsigned char *)p+*offset,n-*offset,MSG_DONTWAIT);
        if(k<0&&errno==EINTR)continue;
        if(k<0&&(errno==EAGAIN||errno==EWOULDBLOCK))return 0;
        if(k<=0) { if(!k)errno=ECONNRESET; return -1; }
        *offset+=(size_t)k;
    }
    return 1;
}
/* Compatibility APIs intentionally wait for completion even on O_NONBLOCK.
 * Use the cursor variants in an event loop. Fatal errors invalidate the stream. */
static inline int mx11_write(int fd,const void *p,size_t n) {
    size_t offset=0;
    int r;
    while((r=mx11_write_step(fd,p,n,&offset))==0)
        if(!mx11_wait(fd,POLLOUT))return 0;
    return r==1;
}
static inline int mx11_read(int fd,void *p,size_t n) {
    size_t offset=0;
    int r;
    while((r=mx11_read_step(fd,p,n,&offset))==0)
        if(!mx11_wait(fd,POLLIN))return 0;
    return r==1;
}
/* Validate all variable-size setup-success screen/depth/visual tables before
 * exposing the borrowed setup body to a caller. No visual format is selected. */
static inline int mulib_x11_wire_setup_body_valid(const unsigned char *p,size_t n) {
    if(!p||n<32||!mx11_get32(p+8)||!mx11_get16(p+18)||!p[20]||!p[21]||
       (mx11_get32(p+4)&mx11_get32(p+8)))return 0;
    size_t vendor=((size_t)mx11_get16(p+16)+3)&~(size_t)3;
    size_t pos=32+vendor;
    if(pos>n||(size_t)p[21]>(n-pos)/8)return 0;
    pos+=(size_t)p[21]*8;
    for(unsigned i=0;i<p[20];i++) {
        if(n-pos<40)return 0;
        unsigned depths=p[pos+39];
        pos+=40;
        for(unsigned d=0;d<depths;d++) {
            if(n-pos<8)return 0;
            size_t visuals=mx11_get16(p+pos+2);
            pos+=8;
            if(visuals>(n-pos)/24)return 0;
            pos+=visuals*24;
        }
    }
    return pos==n;
}
static inline int mulib_x11_wire_setup(mx11_connection *c,int fd,const void *cookie,
        size_t n,unsigned char **reply,size_t *reply_n) {
    if(reply)*reply=NULL;
    if(reply_n)*reply_n=0;
    if(!c||!reply||!reply_n||n>65535||(!cookie&&n)) { errno=EINVAL; return 0; }
    const char *name=n?"MIT-MAGIC-COOKIE-1":"";
    size_t a=strlen(name),ap=(a+3)&~(size_t)3,bp=(n+3)&~(size_t)3;
    unsigned char *req=(unsigned char *)calloc(12+ap+bp,1);
    if(!req)return 0;
    req[0]='l'; mx11_put16(req+2,11);
    mx11_put16(req+6,(uint16_t)a); mx11_put16(req+8,(uint16_t)n);
    memcpy(req+12,name,a);
    if(n)memcpy(req+12+ap,cookie,n);
    int ok=mx11_write(fd,req,12+ap+bp);
    free(req);
    unsigned char pre[8];
    if(!ok||!mx11_read(fd,pre,8))return 0;
    size_t len=(size_t)mx11_get16(pre+6)*4;
    unsigned char *body=(unsigned char *)malloc(len?len:1);
    if(!body)return 0;
    if(!mx11_read(fd,body,len)) {
        int e=errno; free(body); errno=e; return 0;
    }
    if(pre[0]!=1||mx11_get16(pre+2)!=11||mx11_get16(pre+4)!=0||
       !mulib_x11_wire_setup_body_valid(body,len)) {
        free(body); errno=EPROTO; return 0;
    }
    *c=(mx11_connection){.fd=fd,.resource_base=mx11_get32(body+4),
        .resource_mask=mx11_get32(body+8),.max_request=mx11_get16(body+18)};
    *reply=body; *reply_n=len;
    return 1;
}
/* Saturates on exhaustion: no wraparound can reissue an existing resource ID. */
static inline uint32_t mulib_x11_wire_id(mx11_connection *c) {
    if(!c||c->failed||!c->resource_mask||(c->resource_base&c->resource_mask)||
       c->next==UINT32_MAX) { errno=ENOSPC; return 0; }
    uint32_t candidate=c->next+1,n=candidate,result=c->resource_base;
    for(unsigned i=0;i<32;i++)if(c->resource_mask&(UINT32_C(1)<<i)) {
        if(n&1)result|=UINT32_C(1)<<i;
        n>>=1;
    }
    if(n) { errno=ENOSPC; return 0; }
    c->next=candidate;
    return result;
}

typedef struct {
    unsigned char *data;
    size_t n,offset;
    mx11_connection *connection;
    int complete,failed;
} mx11_request_tx;
typedef mx11_request_tx mulib_x11_wire_request_tx_t;
static inline void mulib_x11_wire_request_free(mx11_request_tx *t) {
    if(!t)return;
    if(t->connection&&t->offset&&!t->complete)t->connection->failed=1;
    free(t->data); memset(t,0,sizeof *t);
}
static inline int mulib_x11_wire_request_begin(mx11_request_tx *t,mx11_connection *c,
        uint8_t opcode,uint8_t arg,const void *p,size_t n) {
    if(!t||t->data||!c||c->failed||!opcode||(!p&&n)||n%4||
       n>262136||n/4+1>c->max_request) { errno=EINVAL; return 0; }
    unsigned char *data=(unsigned char *)malloc(n+4);
    if(!data)return 0;
    data[0]=opcode; data[1]=arg; mx11_put16(data+2,(uint16_t)(n/4+1));
    if(n)memcpy(data+4,p,n);
    *t=(mx11_request_tx){.data=data,.n=n+4,.connection=c};
    return 1;
}
static inline int mulib_x11_wire_request_step(mx11_request_tx *t) {
    if(!t||!t->data||!t->connection||t->failed||t->connection->failed) {
        errno=EINVAL; return -1;
    }
    if(t->complete)return 1;
    int r=mx11_write_step(t->connection->fd,t->data,t->n,&t->offset);
    if(r<0) { t->failed=1; t->connection->failed=1; }
    if(r==1) { t->complete=1; t->connection->sequence++; }
    return r;
}
static inline int mulib_x11_wire_request(mx11_connection *c,uint8_t opcode,
        uint8_t arg,const void *p,size_t n) {
    mx11_request_tx t={0};
    if(!mulib_x11_wire_request_begin(&t,c,opcode,arg,p,n))return 0;
    int r;
    while((r=mulib_x11_wire_request_step(&t))==0)
        if(!mx11_wait(c->fd,POLLOUT)) { r=-1; break; }
    int e=errno;
    mulib_x11_wire_request_free(&t);
    errno=e;
    return r==1;
}
/* Replies, errors, fixed events and XGE events. This is framing only; a caller
 * must demultiplex sequence numbers and extensions, and retain partial bytes. */
typedef struct {
    const unsigned char *data;
    size_t consumed;
    uint16_t sequence;
    uint8_t type;
} mx11_frame;
typedef mx11_frame mulib_x11_wire_frame_t;
static inline int mulib_x11_wire_parse(const void *p,size_t n,size_t limit,mx11_frame *out) {
    if(!out||(!p&&n)) { errno=EINVAL; return -1; }
    if(!limit)limit=16777216;
    if(limit<32) { errno=EINVAL; return -1; }
    if(n<32)return 0;
    const unsigned char *b=(const unsigned char *)p;
    uint8_t type=b[0]&127;
    size_t bytes=32;
    if(type==1||type==35) {
        uint32_t extra=mx11_get32(b+4);
        if(extra>(limit-32)/4) { errno=EMSGSIZE; return -1; }
        bytes+=(size_t)extra*4;
    }
    if(bytes>n)return 0;
    *out=(mx11_frame){b,bytes,mx11_get16(b+2),type};
    return 1;
}
#endif
