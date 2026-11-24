/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_SEAT_H
#define MULIB_SEAT_H
#include "mulib-wm-core.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
/* Device ownership abstraction; caller supplies logind/libseat broker or an already-authorized direct opener. */
typedef int (*mulib_seat_open_fn)(void *,const char *,int);
typedef void (*mulib_seat_close_fn)(void *,int);
typedef struct {uint64_t id;int fd;char *path;} mulib_seat_device;
typedef struct {mulib_seat_device *v;size_t n,cap;uint64_t next;int active;void *user;mulib_seat_open_fn open;mulib_seat_close_fn close;} mulib_seat;
static inline uint64_t mulib_seat_acquire(mulib_seat *s,const char *path,int flags){if(!s||!s->active||!s->open||!s->close||!path||!*path||s->next==UINT64_MAX||s->n==SIZE_MAX)return 0;char *p=mulib_wm_strdup(path);if(!p||!mulib_wm_grow((void**)&s->v,&s->cap,s->n+1,sizeof *s->v)){free(p);return 0;}int fd=s->open(s->user,path,flags);if(fd<0){free(p);return 0;}int f=fcntl(fd,F_GETFD);if(f<0||fcntl(fd,F_SETFD,f|FD_CLOEXEC)<0){s->close(s->user,fd);free(p);return 0;}uint64_t id=++s->next;s->v[s->n++]=(mulib_seat_device){id,fd,p};return id;}
static inline int mulib_seat_fd(const mulib_seat *s,uint64_t id){if(s&&s->active)for(size_t i=0;i<s->n;i++)if(s->v[i].id==id)return s->v[i].fd;return -1;}
static inline void mulib_seat_deactivate(mulib_seat *s){if(s){s->active=0;for(size_t i=0;i<s->n;i++){if(s->close)s->close(s->user,s->v[i].fd);free(s->v[i].path);}s->n=0;}}
static inline void mulib_seat_free(mulib_seat *s){if(s){mulib_seat_deactivate(s);free(s->v);memset(s,0,sizeof *s);}}
#endif
