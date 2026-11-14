#include "on1x_runtime.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#ifndef ON1X_ENV_MALLOC
#define ON1X_ENV_MALLOC malloc
#endif
#ifndef ON1X_ENV_FREE
#define ON1X_ENV_FREE free
#endif
static void environment_error(const char *what){fprintf(stderr,"On1x closure error: %s\n",what);exit(72);}
on1x_env *on1x_env_alloc(size_t size,void (*destroy)(on1x_env *)){
 on1x_env *environment;
 if(size<sizeof(on1x_env))environment_error("invalid environment size");
 environment=(on1x_env *)ON1X_ENV_MALLOC(size);
 if(!environment)environment_error("environment allocation failure");
 environment->references=1;environment->destroy=destroy;environment->next=NULL;
 return environment;
}
void on1x_env_retain(on1x_env *environment){
 if(!environment)return;
 if(!environment->references||environment->references==SIZE_MAX)
  environment_error("environment reference overflow or invalid retain");
 ++environment->references;
}
void on1x_env_release(on1x_env *environment){
 static on1x_env *pending;
 static bool draining;
 if(!environment)return;
 if(!environment->references)environment_error("environment reference underflow");
 if(--environment->references)return;
 environment->next=pending;pending=environment;
 if(draining)return;
 /* Compiler-generated destructors only release captured function values.
  * Queue cascades rather than recursively destroying long capture chains. */
 draining=true;
 while(pending){
  on1x_env *current=pending;pending=current->next;
  if(current->destroy)current->destroy(current);
  ON1X_ENV_FREE(current);
 }
 draining=false;
}
static void arithmetic_error(const char *what){fprintf(stderr,"On1x arithmetic error: %s\n",what);exit(70);}
int64_t on1x_add_i64(int64_t a,int64_t b){
#if defined(__GNUC__) || defined(__clang__)
 int64_t c;if(__builtin_add_overflow(a,b,&c))arithmetic_error("addition overflow");return c;
#else
 if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))arithmetic_error("addition overflow");return a+b;
#endif
}
int64_t on1x_sub_i64(int64_t a,int64_t b){
#if defined(__GNUC__) || defined(__clang__)
 int64_t c;if(__builtin_sub_overflow(a,b,&c))arithmetic_error("subtraction overflow");return c;
#else
 if((b<0&&a>INT64_MAX+b)||(b>0&&a<INT64_MIN+b))arithmetic_error("subtraction overflow");return a-b;
#endif
}
int64_t on1x_mul_i64(int64_t a,int64_t b){
#if defined(__GNUC__) || defined(__clang__)
 int64_t c;if(__builtin_mul_overflow(a,b,&c))arithmetic_error("multiplication overflow");return c;
#else
 if(a==0||b==0)return 0;
 if(a==-1&&b==INT64_MIN)arithmetic_error("multiplication overflow");
 if(b==-1&&a==INT64_MIN)arithmetic_error("multiplication overflow");
 if(a>0){if(b>0){if(a>INT64_MAX/b)arithmetic_error("multiplication overflow");}
 else if(b<INT64_MIN/a)arithmetic_error("multiplication overflow");}
 else if(b>0){if(a<INT64_MIN/b)arithmetic_error("multiplication overflow");}
 else if(a<INT64_MAX/b)arithmetic_error("multiplication overflow");
 return a*b;
#endif
}
int64_t on1x_div_i64(int64_t a,int64_t b){if(!b||(a==INT64_MIN&&b==-1))arithmetic_error("invalid division");return a/b;}
int64_t on1x_mod_i64(int64_t a,int64_t b){if(!b||(a==INT64_MIN&&b==-1))arithmetic_error("invalid modulo");return a%b;}
int64_t on1x_neg_i64(int64_t a){if(a==INT64_MIN)arithmetic_error("negation overflow");return -a;}
void on1x_print_i64(int64_t a){printf("%" PRId64 "\n",a);}
void on1x_print_bool(bool a){puts(a?"true":"false");}
void on1x_print_string(const char *a){puts(a?a:"");}
void on1x_assert(bool a){if(!a){fputs("On1x assertion failed\n",stderr);exit(70);}}
