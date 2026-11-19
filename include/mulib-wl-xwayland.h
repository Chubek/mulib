/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_XWAYLAND_H
#define MULIB_WL_XWAYLAND_H
#include "mulib-wm-core.h"
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
/* Supervisor for an externally configured Xwayland child; no implicit X socket setup. */
typedef struct {pid_t pid;int running,status;uint32_t display;int ready;} mulib_wl_xwayland;
static inline int mulib_wl_xwayland_spawn(mulib_wl_xwayland *x,const char *exe,char *const argv[],char *const envp[]){if(!x||x->running||!exe||exe[0]!='/'||!argv||!argv[0]||!envp)return 0;pid_t p=fork();if(p<0)return 0;if(!p){execve(exe,argv,envp);_exit(127);}*x=(mulib_wl_xwayland){.pid=p,.running=1};return 1;}
/* Parse complete displayfd line; caller reads nonblocking and bounds accumulation. */
static inline int mulib_wl_xwayland_ready(mulib_wl_xwayland *x,const char *p,size_t n){if(!x||!x->running||!p||n<2||n>11||p[n-1]!='\n')return 0;uint32_t v=0;for(size_t i=0;i<n-1;i++){if(p[i]<'0'||p[i]>'9'||v>(UINT32_MAX-(unsigned)(p[i]-'0'))/10)return 0;v=v*10+(unsigned)(p[i]-'0');}x->display=v;x->ready=1;return 1;}
static inline int mulib_wl_xwayland_poll(mulib_wl_xwayland *x){if(!x||!x->running)return 0;int s;pid_t p;do{p=waitpid(x->pid,&s,WNOHANG);}while(p<0&&errno==EINTR);if(p<0){if(errno==ECHILD){x->running=x->ready=0;}return -1;}if(!p)return 0;x->status=s;x->running=x->ready=0;return 1;}
static inline int mulib_wl_xwayland_stop(mulib_wl_xwayland *x){return x&&x->running&&x->pid>0&&kill(x->pid,SIGTERM)==0;}
#endif
