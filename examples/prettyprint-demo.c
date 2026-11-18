#include <stdio.h>
#include <stdlib.h>
#define MULIB_IMPL_PRETTYDOC
#define MULIB_IMPL_PRETTYEMIT
#define MULIB_IMPL_PRETTYSEXPR
#include "mulib-prettysexpr.h"
int main(void){
 const char *doc="(cat (annot \"keyword\" (text \"let\")) (cat (soft) (cat (annot \"identifier\" (text \"x\")) (cat (text \" = \") (annot \"math-number\" (text \"42\"))))))";
 mulib_pretty_options_t opt;mulib_pretty_options_init(&opt);opt.width=80;
 for(int target=0;target<=3;target++){
  char *result=mulib_pretty_render_expr(doc,&opt,(mulib_pretty_target_t)target);
  if(!result)return 1;
  printf("===== target %d =====\n%s\n",target,result);free(result);
 }
 return 0;
}
