/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_SERVER_H
#define MULIB_WL_SERVER_H
#include "mulib-wl-registry.h"
#include <sys/un.h>
#include <sys/stat.h>
typedef struct {int fd;char path[sizeof(((struct sockaddr_un*)0)->sun_path)];mulib_wl_registry registry;} mulib_wl_server;
/* Explicit trusted runtime directory; refuses replacing an existing socket. */
static inline int mulib_wl_server_open(mulib_wl_server *s,const char *path){if(!s||!path||!*path||strlen(path)>=sizeof s->path)return 0;int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return 0;struct sockaddr_un a={0};a.sun_family=AF_UNIX;memcpy(a.sun_path,path,strlen(path)+1);if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0||fcntl(fd,F_SETFL,O_NONBLOCK)<0||bind(fd,(struct sockaddr*)&a,sizeof a)<0){close(fd);return 0;}if(chmod(path,0600)<0||listen(fd,32)<0){close(fd);unlink(path);return 0;}*s=(mulib_wl_server){.fd=fd};memcpy(s->path,path,strlen(path)+1);return 1;}
static inline int mulib_wl_display_request(mulib_wl_client *c,mulib_wl_resource *r,uint16_t op,const unsigned char *p,size_t n){if(n!=4)return 0;uint32_t id=mulib_wl_u32(p);if(id<2||id>=0xff000000u||mulib_wl_resource_find(&c->objects,id))return 0;mulib_wl_registry *reg=(mulib_wl_registry*)r->user;if(op==0){uint32_t serial=0;return mulib_wl_client_event(c,id,0,&serial,4)&&mulib_wl_delete_id(c,id);}if(op==1){if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){id,1,"wl_registry",mulib_wl_registry_request,reg,NULL}))return 0;for(size_t i=0;i<reg->n;i++)if(!mulib_wl_registry_announce(c,id,reg->v+i))return 0;return 1;}return 0;}
/* Accepted client borrows server registry; close clients before server. */
static inline int mulib_wl_server_accept(mulib_wl_server *s,mulib_wl_client *c,size_t limit){if(!s||!c||s->fd<0)return -1;int fd=accept(s->fd,NULL,NULL);if(fd<0)return errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR?0:-1;if(!mulib_wl_client_init(c,fd,limit)){close(fd);return -1;}if(!mulib_wl_resource_add(&c->objects,(mulib_wl_resource){1,1,"wl_display",mulib_wl_display_request,&s->registry,NULL})){mulib_wl_client_free(c);return -1;}return 1;}
static inline void mulib_wl_server_free(mulib_wl_server *s){if(s){if(s->path[0]){close(s->fd);unlink(s->path);}mulib_wl_registry_free(&s->registry);memset(s,0,sizeof *s);s->fd=-1;}}
#endif
