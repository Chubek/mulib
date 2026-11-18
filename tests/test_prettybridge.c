#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../on1x/stdlib/prettyprint/runtime.h"

/* The generic focused-test runner compiles one source. The PrettyPrint build
 * also tests this caller with a separate runtime and implementation object. */
#ifndef PRETTYPRINT_TEST_SEPARATE
#define MULIB_IMPL_PRETTYDOC
#define MULIB_IMPL_PRETTYEMIT
#define MULIB_IMPL_PRETTYSEXPR
#include "mulib-prettysexpr.h"
#include "../on1x/stdlib/prettyprint/runtime.c"
#endif

_Static_assert(_Generic(&on1x_pretty_render,
 const char *(*)(const char *,int64_t,int64_t,int64_t,int64_t):1,default:0),"render ABI");
_Static_assert(_Generic(&on1x_pretty_status,int64_t (*)(void):1,default:0),"status ABI");

int main(void){
 const char *src="(cat (text \"A\") (text \"B\"))";
 const char *a=on1x_pretty_render(src,80,1,0,0);
 assert(on1x_pretty_status()==0&&strcmp(a,"AB")==0);
 const char *b=on1x_pretty_render("(unknown)",80,1,0,0);
 assert(on1x_pretty_status()==-2&&strcmp(b,"")==0);
 b=on1x_pretty_render("(text \"\")",80,1,0,0);
 assert(on1x_pretty_status()==0&&strcmp(b,"")==0);
 b=on1x_pretty_render(NULL,80,1,0,0);
 assert(on1x_pretty_status()==-1&&strcmp(b,"")==0);
 b=on1x_pretty_render(src,INT64_MAX,1,0,0);
 assert(on1x_pretty_status()==-1&&strcmp(b,"")==0);
 b=on1x_pretty_render(src,80,1,0,-1);
 assert(on1x_pretty_status()==-1&&strcmp(b,"")==0);
 size_t limit=2u*1024u*1024u;
 char *large=malloc(limit+2);assert(large);
 memset(large,' ',limit+1);memcpy(large,src,strlen(src));large[limit]='\0';
 b=on1x_pretty_render(large,80,1,0,0);
 assert(on1x_pretty_status()==0&&strcmp(b,"AB")==0);
 large[limit]=' ';large[limit+1]='\0';
 b=on1x_pretty_render(large,80,1,0,0);
 assert(on1x_pretty_status()==-4&&strcmp(b,"")==0);

 /* Produce enough successful snapshots to reach the process pool limit. */
 size_t text=512u*1024u;
 memcpy(large,"(text \"",7);
 memset(large+7,'z',text);
 memcpy(large+7+text,"\")",3);
 unsigned saved=0;
 while(saved<32){
  b=on1x_pretty_render(large,80,1,0,0);
  if(on1x_pretty_status()!=0)break;
  assert(strlen(b)==text);
  saved++;
 }
 assert(saved==31 && on1x_pretty_status()==-4 && strcmp(b,"")==0);
 free(large);
 assert(strcmp(a,"AB")==0); /* snapshots survive subsequent calls */
 puts("On1x PrettyPrint native ABI tests passed");return 0;
}
