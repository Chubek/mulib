/* Check the private C11 runtime boundary with closed parent descriptors. */
#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_CHANNEL
#define MULIB_IMPL_FIBER
#define MULIB_IMPL_XTHREADS
#include <mulib-channel.h>
#include "on1x_concurrency.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define AT __FILE__, __LINE__, 1
#define CHECK(test) do { if (!(test)) { \
 fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); \
 exit(1); \
} } while (0)

static const char *child;
static int descriptor_flags[3];

static void check_descriptors(void){
 for(int fd=0;fd<3;++fd){
  CHECK(fcntl(fd,F_GETFD)==descriptor_flags[fd]);
  if(descriptor_flags[fd]==-1)CHECK(errno==EBADF);
 }
}

static int64_t capture(on1x_env *env){
 const char *argv[]={child,"stream","two words","four","escaped word","#literal",NULL};
 on1x_csp_process *process;
 on1x_csp_endpoint *input,*output,*error;
 const uint8_t sent=0;
 uint8_t received;
 (void)env;
 on1x_csp_group *group=on1x_csp_group_enter(AT);
 on1x_csp_launch_capture(argv,6,&process,&input,&output,&error,AT);
 check_descriptors();
 CHECK(on1x_csp_send(input,&sent,sizeof(sent),AT)==ON1X_CSP_TRANSFERRED);
 CHECK(on1x_csp_close(input,AT));
 CHECK(on1x_csp_receive(output,&received,sizeof(received),AT)&&received==sent);
 CHECK(!on1x_csp_receive(output,&received,sizeof(received),AT));
 for(const char *p="ERR";*p;++p)
  CHECK(on1x_csp_receive(error,&received,sizeof(received),AT)&&received==(uint8_t)*p);
 CHECK(!on1x_csp_receive(error,&received,sizeof(received),AT));
 CHECK(on1x_csp_process_join(process,AT)==0);
 on1x_csp_group_finish(group);
 return 0;
}

int main(int argc,char **argv){
 CHECK(argc==3&&argv[1][0]>='0'&&argv[1][0]<='7'&&argv[1][1]=='\0');
 unsigned mask=(unsigned)(argv[1][0]-'0');
 child=argv[2];
 for(int fd=0;fd<3;++fd){
  descriptor_flags[fd]=fcntl(fd,F_GETFD);
  CHECK(descriptor_flags[fd]>=0);
  if(mask&(1u<<fd)){
   CHECK(close(fd)==0);
   descriptor_flags[fd]=-1;
  }
 }
 CHECK(on1x_csp_run_v5(capture,NULL,AT)==0);
 check_descriptors();
 return 0;
}
