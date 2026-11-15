#define _POSIX_C_SOURCE 200809L
#include "../runtime/basis_native.h"
#include <assert.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
static void expect_failure(int scenario){
  pid_t pid=fork();assert(pid>=0);
  if(pid==0){
    if(scenario==0){int64_t a=basis_native_arr_new(2,0);basis_native_arr_get(a,2);}
    if(scenario==1){int64_t a=basis_native_arr_new(2,0);basis_native_arr_free(a);basis_native_arr_get(a,0);}
    if(scenario==2){basis_native_arr_new(-1,0);}
    if(scenario==3){basis_native_arr_new(262145,0);}
    if(scenario==4){basis_native_real_div("1","0");}
    if(scenario==5){basis_native_math_sqrt("-1");}
    if(scenario==6){basis_native_real_add("nan","1");}
    _exit(0);
  }
  int status=0;assert(waitpid(pid,&status,0)==pid);
  assert(WIFEXITED(status));assert(WEXITSTATUS(status)==73);
}
int main(void){for(int i=0;i<7;++i)expect_failure(i);puts("basis negative tests passed");return 0;}
