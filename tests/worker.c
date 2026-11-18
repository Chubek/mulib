#include "mulib-sandbox-process.h"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>
int main(void) {
    /* trusted bootstrap checks before untrusted input. */
    if(getenv("MULIB_SECRET")||getenv("LD_PRELOAD")||getenv("PATH")||fcntl(192,F_GETFD)!=-1||errno!=EBADF)_exit(126);
#ifdef FAKE_READY
    mulib_ipc_framed_t ipc;int64_t deadline;unsigned char hello[8]={'S','B','0','1',0,0,0,MULIB_SB_BASELINE};
    if(mulib_ipc_framed_init(&ipc,3,1024)||mulib_ipc_deadline(1000,&deadline)||mulib_ipc_send(&ipc,hello,8,deadline))_exit(127);
    sleep(5);return 0;
#else
    mulib_ipc_framed_t ipc;mulib_sandbox_profile_t p;
    if(mulib_sandbox_worker_enter(&ipc,&p))_exit(125);
    int64_t deadline;if(mulib_ipc_deadline(p.wall_ms,&deadline))_exit(127);
    unsigned char buf[1024];size_t n;if(mulib_ipc_receive(&ipc,buf,sizeof buf,&n,deadline))_exit(3);
    if(n==1&&buf[0]=='T'){
        if(mulib_ipc_send(&ipc,buf,n,deadline)||mulib_ipc_receive(&ipc,buf,sizeof buf,&n,deadline)||n!=1||buf[0]!='T')_exit(8);
        _exit(9);
    }
    if(n&&buf[0]=='F'){(void)open("/etc/passwd",O_RDONLY);_exit(4);}
    if(n&&buf[0]=='N'){(void)socket(AF_INET,SOCK_STREAM,0);_exit(4);}
    if(n&&buf[0]=='P'){(void)fork();_exit(4);}
    if(n&&buf[0]=='X'){(void)mmap(NULL,4096,PROT_READ|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);_exit(4);}
    if(n&&buf[0]=='S'){(void)write(1,"bad",3);_exit(4);}
    if(n&&buf[0]=='D'){(void)dup(3);_exit(4);}
    if(n&&buf[0]=='E'){char *const args[]={(char*)"true",NULL};execve("/bin/true",args,args);_exit(4);}
    if(n&&(buf[0]=='L'||buf[0]=='C')){for(;;){}}
    if(n&&buf[0]=='M'){void *mem=mmap(NULL,(size_t)p.memory_bytes*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(mem!=MAP_FAILED)_exit(5);}
    /* Exercise private malloc and basic allowed IPC. */
    void *mem=malloc(65536);if(!mem)_exit(6);memset(mem,123,65536);free(mem);
    if(mulib_ipc_send(&ipc,buf,n,deadline))_exit(7);
    _exit(0);
#endif
}
