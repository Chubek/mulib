/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_WORKER_H
#define MULIB_NB_WORKER_H
#include "mulib-wm-core.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
/* Trusted worker bootstrap only. NOT a sandbox. Use mulib-sandbox-process for untrusted code. */
typedef struct {pid_t pid;int stdout_fd,stderr_fd,running,status;} mulib_nb_worker;
#define MULIB_NB_WORKER_INIT { .stdout_fd = -1, .stderr_fd = -1 }
static inline int mulib_nb_worker_pipe(int p[2]){if(pipe(p)<0)return 0;for(int i=0;i<2;i++){if(p[i]<3){int z=fcntl(p[i],F_DUPFD_CLOEXEC,3);if(z<0){close(p[0]);close(p[1]);return 0;}close(p[i]);p[i]=z;}if(fcntl(p[i],F_SETFD,FD_CLOEXEC)<0){close(p[0]);close(p[1]);return 0;}}int f=fcntl(p[0],F_GETFL);if(f<0||fcntl(p[0],F_SETFL,f|O_NONBLOCK)<0){close(p[0]);close(p[1]);return 0;}return 1;}
static inline int mulib_nb_worker_start(mulib_nb_worker *w,const char *exe,char *const argv[],char *const envp[],int authorized){if(!w||w->running||w->stdout_fd>=0||w->stderr_fd>=0||!authorized||!exe||exe[0]!='/'||!argv||!argv[0]||!envp)return 0;int o[2],e[2];if(!mulib_nb_worker_pipe(o))return 0;if(!mulib_nb_worker_pipe(e)){close(o[0]);close(o[1]);return 0;}pid_t pid=fork();if(pid==0){close(o[0]);close(e[0]);if(dup2(o[1],1)<0||dup2(e[1],2)<0)_exit(126);close(o[1]);close(e[1]);execve(exe,argv,envp);_exit(127);}close(o[1]);close(e[1]);if(pid<0){close(o[0]);close(e[0]);return 0;}*w=(mulib_nb_worker){pid,o[0],e[0],1,0};return 1;}
static inline ssize_t mulib_nb_worker_read(mulib_nb_worker *w,int err,void *p,size_t n){if(!w||!p||!n){errno=EINVAL;return -1;}int *fd=err?&w->stderr_fd:&w->stdout_fd;if(*fd<0)return 0;ssize_t r;do{r=read(*fd,p,n);}while(r<0&&errno==EINTR);if(!r){close(*fd);*fd=-1;}return r;}
static inline int mulib_nb_worker_poll(mulib_nb_worker *w){if(!w||!w->running)return 0;int st;pid_t p;do{p=waitpid(w->pid,&st,WNOHANG);}while(p<0&&errno==EINTR);if(p<0){if(errno==ECHILD)w->running=0;return -1;}if(!p)return 0;w->status=st;w->running=0;return 1;}
static inline int mulib_nb_worker_interrupt(mulib_nb_worker *w,int signal_number){return w&&w->running&&w->pid>0&&kill(w->pid,signal_number)==0;}
static inline int mulib_nb_worker_close(mulib_nb_worker *w){if(!w||w->running)return 0;if(w->stdout_fd>=0)close(w->stdout_fd);if(w->stderr_fd>=0&&w->stderr_fd!=w->stdout_fd)close(w->stderr_fd);w->stdout_fd=w->stderr_fd=-1;return 1;}
#endif
