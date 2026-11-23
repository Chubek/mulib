/* Build: cc -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
 *  -Iinclude examples/process_stream.c -o /tmp/mulib-process-stream-example
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define MULIB_IMPL_PROCESS_STREAM
#include "mulib-process-stream.h"
#include <stdio.h>
static int output(void *u,int channel,const unsigned char *data,size_t n){
    (void)u;
    FILE *dest=channel==2?stderr:stdout;
    return fwrite(data,1,n,dest)==n;
}
int main(void){
    const char *const argv[]={"/bin/sh","-c","printf 'stdout\\n'; printf 'stderr\\n' >&2",NULL};
    mulib_ps_spec_t spec={"/bin/sh",argv,NULL,NULL,0,0,0,0,5000,65536};
    mulib_ps_t proc=MULIB_PS_INIT;
    if(mulib_ps_start(&proc,&spec)!=MULIB_PS_RUNNING){perror("spawn");return 1;}
    int rc=mulib_ps_wait(&proc,output,NULL);
    int status=proc.exit_code;
    mulib_ps_destroy(&proc);
    return rc==MULIB_PS_DONE?status:1;
}
