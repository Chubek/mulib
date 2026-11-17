/* Independent strict-C11 caller; exactly one TU owns these implementation switches. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
static int fail_next_allocation;
static void *test_calloc(size_t count,size_t size){
 if(fail_next_allocation){fail_next_allocation=0;return NULL;}
 return calloc(count,size);
}
#define calloc test_calloc
#define MULIB_IMPL_CHANNEL
#define MULIB_IMPL_FIBER
#define MULIB_IMPL_XTHREADS
#include <mulib-channel.h>
#undef calloc
#include "on1x_concurrency.h"
#include <assert.h>

int on1x_test_poll_mode;
int64_t on1x_test_poll(on1x_env *env){
 on1x_csp_process *process;
 on1x_csp_channel *input,*output,*error;
 on1x_csp_group *group;
 const char *argv[]={"/bin/cat",NULL};
 on1x_csp_offer offer;
 int64_t payload;size_t selected;bool closed;
 (void)env;
 group=on1x_csp_group_enter(__FILE__,1,1);
 on1x_csp_launch_capture(argv,1,&process,&input,&output,&error,__FILE__,2,1);
 offer.channel=input;offer.operation=MULIB_CHANNEL_SEND;offer.payload=255;
 if(on1x_test_poll_mode==1)offer.payload=256;
 if(on1x_test_poll_mode==2)offer.payload=-1;
 if(on1x_test_poll_mode==3)offer.channel=output;
 if(on1x_test_poll_mode==4)offer.operation=MULIB_CHANNEL_RECEIVE;
 if(on1x_test_poll_mode==5){
  offer.channel=output;offer.operation=MULIB_CHANNEL_RECEIVE;
  fail_next_allocation=1;
 }
 on1x_csp_select(&offer,1,false,&payload,&selected,&closed,__FILE__,3,1);
 assert(selected==0&&!closed);
 (void)on1x_csp_stream_close(input,__FILE__,4,1);
 offer.channel=output;offer.operation=MULIB_CHANNEL_RECEIVE;
 on1x_csp_select(&offer,1,false,&payload,&selected,&closed,__FILE__,5,1);
 assert(selected==0&&!closed&&payload==255);
 on1x_csp_select(&offer,1,false,&payload,&selected,&closed,__FILE__,6,1);
 assert(selected==0&&closed&&payload==0);
 offer.channel=error;
 on1x_csp_select(&offer,1,false,&payload,&selected,&closed,__FILE__,7,1);
 assert(selected==0&&closed&&payload==0);
 assert(on1x_csp_process_join(process,__FILE__,8,1)==0);
 on1x_csp_group_finish(group);
 return 0;
}
