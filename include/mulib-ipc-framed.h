/** @file mulib-ipc-framed.h
 * Bounded big-endian u32 framing over connected POSIX stream sockets, absolute
 * monotonic deadlines. Borrowed fd; poisoned streams must be closed by owner.
 * Define MULIB_IMPL_IPC_FRAMED and MULIB_IMPL_SANDBOX_PROFILE once.
 */
#ifndef MULIB_IPC_FRAMED_H
#define MULIB_IPC_FRAMED_H
#include "mulib-sandbox-profile.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_ipc_framed { int fd, poisoned; uint32_t max_frame; } mulib_ipc_framed_t;
/** Initialize a borrowed connected stream. Not thread-safe. */
int mulib_ipc_framed_init(mulib_ipc_framed_t *s,int fd,uint32_t max_frame);
/** Milliseconds on CLOCK_MONOTONIC; -1 on failure. */
int64_t mulib_ipc_now_ms(void);
/** Calculate an absolute deadline (no infinite deadlines). */
int mulib_ipc_deadline(uint64_t timeout_ms,int64_t *out);
int mulib_ipc_send(mulib_ipc_framed_t *s,const void *data,size_t length,int64_t deadline);
/** Caller-owned buffer. *length unchanged on error. Truncation is PROTOCOL;
 * EOF is a peer shutdown before the next frame header starts. */
int mulib_ipc_receive(mulib_ipc_framed_t *s,void *data,size_t capacity,size_t *length,int64_t deadline);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_IPC_FRAMED) && !defined(MULIB_IPC_FRAMED_IMPLEMENTED)
#define MULIB_IPC_FRAMED_IMPLEMENTED
#include <errno.h>
#include <limits.h>
#if defined(__unix__) || defined(__APPLE__)
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
int64_t mulib_ipc_now_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0||t.tv_nsec<0||t.tv_nsec>=1000000000L)return -1; uint64_t frac=(uint64_t)t.tv_nsec/1000000u;if((uint64_t)t.tv_sec>((uint64_t)INT64_MAX-frac)/1000u)return -1;return (int64_t)((uint64_t)t.tv_sec*1000u+frac);}
int mulib_ipc_deadline(uint64_t ms,int64_t *out){int64_t now=mulib_ipc_now_ms();if(!out||!ms)return MULIB_SB_INVALID;if(now<0)return MULIB_SB_SYSTEM;if(ms>(uint64_t)(INT64_MAX-now))return MULIB_SB_LIMIT;*out=now+(int64_t)ms;return 0;}
int mulib_ipc_framed_init(mulib_ipc_framed_t *s,int fd,uint32_t max){int type;socklen_t n=sizeof type;struct sockaddr_storage peer;socklen_t pn=sizeof peer;if(!s||fd<0||!max||max>64u*1024*1024)return MULIB_SB_INVALID;
    if(getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&n)||type!=SOCK_STREAM||getpeername(fd,(struct sockaddr*)&peer,&pn))return MULIB_SB_INVALID;
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    int one=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof one))return MULIB_SB_SYSTEM;
#elif !defined(MSG_NOSIGNAL)
    return MULIB_SB_UNSUPPORTED;
#endif
    s->fd=fd;s->poisoned=0;s->max_frame=max;return 0;}
static int mulib_ipc_transfer_(mulib_ipc_framed_t *s,void *ptr,size_t len,int write_mode,int64_t end){
    unsigned char *p=(unsigned char*)ptr;
    while(len){int64_t now=mulib_ipc_now_ms();if(now<0)return MULIB_SB_SYSTEM;if(now>=end)return MULIB_SB_TIMEOUT;
        ssize_t n;if(write_mode){int flags=MSG_DONTWAIT;
#ifdef MSG_NOSIGNAL
            flags|=MSG_NOSIGNAL;
#endif
            n=send(s->fd,p,len,flags);
        }else n=recv(s->fd,p,len,MSG_DONTWAIT);
        if(n>0){p+=(size_t)n;len-=(size_t)n;continue;}if(!n)return p==(unsigned char*)ptr?MULIB_SB_EOF:MULIB_SB_PROTOCOL;
        if(errno==EINTR)continue;
        if(errno!=EAGAIN&&errno!=EWOULDBLOCK)return MULIB_SB_SYSTEM;
        struct pollfd f={s->fd,(short)(write_mode?POLLOUT:POLLIN),0};int64_t remain=end-now;
        int rc=poll(&f,1,remain>INT_MAX?INT_MAX:(int)remain);if(rc<0&&errno==EINTR)continue;if(rc<0)return MULIB_SB_SYSTEM;if(!rc)continue;if(f.revents&POLLNVAL)return MULIB_SB_SYSTEM;
    }int64_t now=mulib_ipc_now_ms();return now<0?MULIB_SB_SYSTEM:now>=end?MULIB_SB_TIMEOUT:0;}
int mulib_ipc_send(mulib_ipc_framed_t *s,const void *p,size_t n,int64_t end){if(!s||s->fd<0||(!p&&n)||end<0)return MULIB_SB_INVALID;if(s->poisoned)return MULIB_SB_STATE;if(n>s->max_frame)return MULIB_SB_LIMIT;
    unsigned char h[4]={(unsigned char)(n>>24),(unsigned char)(n>>16),(unsigned char)(n>>8),(unsigned char)n};int rc=mulib_ipc_transfer_(s,h,4,1,end);if(!rc&&n)rc=mulib_ipc_transfer_(s,(void*)p,n,1,end);if(rc)s->poisoned=1;return rc;}
int mulib_ipc_receive(mulib_ipc_framed_t *s,void *p,size_t cap,size_t *n,int64_t end){if(!s||s->fd<0||(!p&&cap)||!n||end<0)return MULIB_SB_INVALID;if(s->poisoned)return MULIB_SB_STATE;unsigned char h[4];int rc=mulib_ipc_transfer_(s,h,4,0,end);if(!rc){uint32_t len=((uint32_t)h[0]<<24)|((uint32_t)h[1]<<16)|((uint32_t)h[2]<<8)|h[3];if(len>s->max_frame||len>cap)rc=MULIB_SB_LIMIT;else{if(len){rc=mulib_ipc_transfer_(s,p,len,0,end);if(rc==MULIB_SB_EOF)rc=MULIB_SB_PROTOCOL;}if(!rc)*n=len;}}if(rc)s->poisoned=1;return rc;}
#else
int64_t mulib_ipc_now_ms(void){return -1;}
int mulib_ipc_deadline(uint64_t a,int64_t*b){(void)a;(void)b;return MULIB_SB_UNSUPPORTED;}
int mulib_ipc_framed_init(mulib_ipc_framed_t*a,int b,uint32_t c){(void)a;(void)b;(void)c;return MULIB_SB_UNSUPPORTED;}
int mulib_ipc_send(mulib_ipc_framed_t*a,const void*b,size_t c,int64_t d){(void)a;(void)b;(void)c;(void)d;return MULIB_SB_UNSUPPORTED;}
int mulib_ipc_receive(mulib_ipc_framed_t*a,void*b,size_t c,size_t*d,int64_t e){(void)a;(void)b;(void)c;(void)d;(void)e;return MULIB_SB_UNSUPPORTED;}
#endif
#endif
