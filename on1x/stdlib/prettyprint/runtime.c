/* Raw ABI companion; the MuLIB link planner owns all kernel implementations. */
#include "runtime.h"
#include <mulib-prettysexpr.h>
#include <stdlib.h>
#include <string.h>

enum { ON1X_PRETTY_MAX_BYTES = 2 * 1024 * 1024, ON1X_PRETTY_POOL_BYTES = 16 * 1024 * 1024 };
struct on1x_pretty_saved {struct on1x_pretty_saved *next;char contents[];};
static struct on1x_pretty_saved *on1x_pretty_pool;
static size_t on1x_pretty_pool_size;
static int on1x_pretty_registered;
static int64_t on1x_pretty_last_status;
static void on1x_pretty_cleanup(void){
 while(on1x_pretty_pool){struct on1x_pretty_saved *s=on1x_pretty_pool;on1x_pretty_pool=s->next;free(s);}
 on1x_pretty_pool_size=0;
}
int64_t on1x_pretty_status(void){return on1x_pretty_last_status;}
const char *on1x_pretty_render(const char *expr,int64_t width,int64_t algorithm,
                               int64_t target,int64_t paragraph){
 on1x_pretty_last_status=-1;
 if(!expr || width<1 || width>4096 || algorithm<0 || algorithm>2 || target<0 || target>3 || paragraph<0 || paragraph>2)
  return "";
 size_t n=0;
 while(n<=ON1X_PRETTY_MAX_BYTES && expr[n])n++;
 if(n>ON1X_PRETTY_MAX_BYTES){on1x_pretty_last_status=-4;return "";}
 mulib_pretty_options_t opt;mulib_pretty_options_init(&opt);
 opt.width=(size_t)width;opt.algorithm=(mulib_pretty_algorithm_t)algorithm;
 opt.paragraph=(mulib_pretty_paragraph_t)paragraph;opt.max_output=ON1X_PRETTY_MAX_BYTES;
 char *s=mulib_pretty_render_expr(expr,&opt,(mulib_pretty_target_t)target);
 if(!s){on1x_pretty_last_status=-2;return "";}
 n=strlen(s);
 if(n>ON1X_PRETTY_MAX_BYTES || n>SIZE_MAX-sizeof(struct on1x_pretty_saved)-1 ||
     sizeof(struct on1x_pretty_saved)+n+1>ON1X_PRETTY_POOL_BYTES-on1x_pretty_pool_size){
  free(s);on1x_pretty_last_status=-4;return "";
 }
 if(!on1x_pretty_registered){
  if(atexit(on1x_pretty_cleanup)){free(s);on1x_pretty_last_status=-3;return "";}
  on1x_pretty_registered=1;
 }
 struct on1x_pretty_saved *saved=(struct on1x_pretty_saved*)malloc(sizeof(*saved)+n+1);
 if(!saved){free(s);on1x_pretty_last_status=-3;return "";}
 saved->next=on1x_pretty_pool;memcpy(saved->contents,s,n+1);free(s);
 on1x_pretty_pool=saved;on1x_pretty_pool_size+=sizeof(*saved)+n+1;
 on1x_pretty_last_status=0;return saved->contents;
}
