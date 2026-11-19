/* Independent strict-C11 caller; exactly one TU owns these implementation switches. */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include "runtime_alloc_hook.h"

size_t on1x_test_runtime_fail_after;
void *on1x_test_runtime_calloc(size_t count,size_t size){
 if(on1x_test_runtime_fail_after&&!--on1x_test_runtime_fail_after)return NULL;
 return calloc(count,size);
}
void on1x_test_runtime_free(void *value){free(value);}

static size_t fail_after_allocation;
static int test_allocation_fails(void){
 return fail_after_allocation&&!--fail_after_allocation;
}
static void *test_calloc(size_t count,size_t size){
 if(test_allocation_fails())return NULL;
 return calloc(count,size);
}
static void *test_malloc(size_t size){
 if(test_allocation_fails())return NULL;
 return malloc(size);
}
#define calloc test_calloc
#define malloc test_malloc
#define MULIB_IMPL_CHANNEL
#define MULIB_IMPL_FIBER
#define MULIB_IMPL_XTHREADS
#include <mulib-channel.h>
#undef calloc
#undef malloc
#include "on1x_concurrency.h"
#include <assert.h>

int on1x_test_poll_mode;
int64_t on1x_test_poll(on1x_env *env){
 on1x_csp_process *process;
 on1x_csp_endpoint *input,*output,*error;
 on1x_csp_group *group;
 const char *argv[]={"/bin/cat",NULL};
 on1x_csp_offer offer;
 uint8_t sent=255,received=0;size_t selected;bool closed;
 (void)env;
 group=on1x_csp_group_enter(__FILE__,1,1);
 if(on1x_test_poll_mode>=7&&on1x_test_poll_mode<=10){
  if(on1x_test_poll_mode<9)
   on1x_test_runtime_fail_after=(size_t)(on1x_test_poll_mode-6);
  else if(on1x_test_poll_mode==9)
   fail_after_allocation=1;
  else
   fail_after_allocation=2;
  (void)on1x_csp_channel_create(
   sizeof(uint8_t),1,1,on1x_test_poll_mode==10?1:0,__FILE__,2,1);
  assert(!"injected channel allocation failure was ignored");
 }
 if(on1x_test_poll_mode>=11&&on1x_test_poll_mode<=16){
  on1x_csp_channel *channel=on1x_csp_channel_create(
   sizeof(uint16_t),2,1,1,__FILE__,2,1);
  on1x_csp_endpoint *endpoints[3]={NULL,NULL,NULL};
  uint16_t value=7;
  if(on1x_test_poll_mode==11)
   on1x_csp_channel_split(channel,endpoints,1,1,__FILE__,3,1);
  on1x_csp_channel_split(channel,endpoints,2,1,__FILE__,3,1);
  if(on1x_test_poll_mode==12)
   on1x_csp_channel_split(channel,endpoints,2,1,__FILE__,3,1);
  if(on1x_test_poll_mode==13){
   on1x_csp_offer duplicate[2]={
    {endpoints[2],MULIB_CHANNEL_RECEIVE,NULL,&value,sizeof(value)},
    {endpoints[2],MULIB_CHANNEL_RECEIVE,NULL,&value,sizeof(value)}
   };
   on1x_csp_select(duplicate,2,true,&selected,&closed,__FILE__,3,1);
  }
  if(on1x_test_poll_mode==14)
   (void)on1x_csp_send(endpoints[2],&value,sizeof(value),__FILE__,3,1);
  if(on1x_test_poll_mode==15)
   (void)on1x_csp_send(endpoints[0],&value,0,__FILE__,3,1);
  if(on1x_test_poll_mode==16)
   (void)on1x_csp_close(endpoints[2],__FILE__,3,1);
  assert(!"malformed ordinary channel ABI call was ignored");
 }
 on1x_csp_launch_capture(argv,1,&process,&input,&output,&error,__FILE__,2,1);
 offer.endpoint=input;offer.operation=MULIB_CHANNEL_SEND;
 offer.send_value=&sent;offer.receive_value=NULL;offer.element_size=sizeof(sent);
 if(on1x_test_poll_mode==1)offer.element_size=sizeof(uint16_t);
 if(on1x_test_poll_mode==6)offer.element_size=0;
 if(on1x_test_poll_mode==2)offer.send_value=NULL;
 if(on1x_test_poll_mode==3)offer.endpoint=output;
 if(on1x_test_poll_mode==4)offer.operation=MULIB_CHANNEL_RECEIVE;
 if(on1x_test_poll_mode==5){
  offer.endpoint=output;offer.operation=MULIB_CHANNEL_RECEIVE;
  offer.send_value=NULL;offer.receive_value=&received;
  fail_after_allocation=1;
 }
 if(on1x_test_poll_mode==4){
  offer.send_value=NULL;offer.receive_value=&received;
 }
 on1x_csp_select(&offer,1,false,&selected,&closed,__FILE__,3,1);
 assert(selected==0&&!closed);
 (void)on1x_csp_close(input,__FILE__,4,1);
 offer.endpoint=output;offer.operation=MULIB_CHANNEL_RECEIVE;
 offer.send_value=NULL;offer.receive_value=&received;offer.element_size=sizeof(received);
 on1x_csp_select(&offer,1,false,&selected,&closed,__FILE__,5,1);
 assert(selected==0&&!closed&&received==255);
 received=99;
 on1x_csp_select(&offer,1,false,&selected,&closed,__FILE__,6,1);
 assert(selected==0&&closed&&received==0);
 offer.endpoint=error;received=99;
 on1x_csp_select(&offer,1,false,&selected,&closed,__FILE__,7,1);
 assert(selected==0&&closed&&received==0);
 assert(on1x_csp_process_join(process,__FILE__,8,1)==0);
 on1x_csp_group_finish(group);
 return 0;
}
