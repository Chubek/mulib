/**
 * @file mulib-prettysexpr.h
 * @brief Parse language-independent PrettyDoc S-expression interchange.
 * Define MULIB_IMPL_PRETTYSEXPR in exactly one translation unit; requires
 * MULIB_IMPL_PRETTYDOC, and MULIB_IMPL_PRETTYEMIT to call render_expr.
 * Supported forms: (text "str"), (fill "paragraph"), (soft), (hard),
 * (cat a b), (group a), (nest unsigned a), (align a),
 * (choice a b), (region "greedy|fits|search" a),
 * (paragraph "greedy|ragged" a), (annot "keyword|identifier|number|operator|comment|string|
 * math-ident|math-number|math-operator" a).
 */
#ifndef MULIB_PRETTYSEXPR_H
#define MULIB_PRETTYSEXPR_H
#include "mulib-prettyemit.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Transactional: out_doc and out_root unchanged on failure. */
int mulib_pretty_parse_expr(const char *source,mulib_prettydoc_t **out_doc,
                            mulib_pretty_id_t *out_root);
char *mulib_pretty_render_expr(const char *source,const mulib_pretty_options_t *options,
                                mulib_pretty_target_t target);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PRETTYSEXPR) && !defined(MULIB_PRETTYSEXPR_IMPLEMENTED)
#define MULIB_PRETTYSEXPR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_pretty_parser_ {
 const char *p;mulib_prettydoc_t *doc;unsigned nodes;
} mulib_pretty_parser_;
static void mulib_pretty_ws_(mulib_pretty_parser_ *p){
 while(*p->p && isspace((unsigned char)*p->p))p->p++;
}
static int mulib_pretty_sym_(mulib_pretty_parser_ *p,char *out,size_t cap){
 size_t n=0;mulib_pretty_ws_(p);
 while((*p->p>='a'&&*p->p<='z')||(*p->p>='A'&&*p->p<='Z')||
       (*p->p>='0'&&*p->p<='9')||*p->p=='-'||*p->p=='_'){
   if(n+1>=cap)return 0;
   out[n++]=*p->p++;
 }
 out[n]=0;return n!=0;
}
static int mulib_pretty_expect_(mulib_pretty_parser_ *p,char c){
 mulib_pretty_ws_(p);if(*p->p!=c)return 0;p->p++;return 1;
}
static int mulib_pretty_hex_(unsigned char c){
 if(c>='0'&&c<='9')return c-'0';
 if(c>='a'&&c<='f')return c-'a'+10;
 if(c>='A'&&c<='F')return c-'A'+10;
 return -1;
}
static char *mulib_pretty_quote_(mulib_pretty_parser_ *p){
 mulib_pretty_ws_(p);if(*p->p!='"')return NULL;p->p++;
 size_t cap=64,len=0;char *s=(char*)malloc(cap);if(!s)return NULL;
 while(*p->p && *p->p!='"'){
  unsigned char ch=(unsigned char)*p->p++;
  if(ch=='\\'){
   char esc=*p->p++;if(!esc){free(s);return NULL;}
    switch(esc){case 'n':ch='\n';break;case 't':ch='\t';break;
     case 'r':ch='\r';break;case '"':ch='"';break;case '\\':ch='\\';break;
     case 'x':{
      int hi=mulib_pretty_hex_((unsigned char)*p->p);
      if(hi<0){free(s);return NULL;}
      p->p++;
      int lo=mulib_pretty_hex_((unsigned char)*p->p);
      if(lo<0){free(s);return NULL;}
      p->p++;ch=(unsigned char)(16*hi+lo);break;
     }
     default:free(s);return NULL;}
  }
  if(!ch || len>=1024*1024){free(s);return NULL;}
  if(len+2>cap){size_t nc=cap*2;char *x=(char*)realloc(s,nc);if(!x){free(s);return NULL;}s=x;cap=nc;}
  s[len++]=(char)ch;
 }
 if(*p->p!='"'){free(s);return NULL;}
 p->p++;s[len]=0;return s;
}
static int mulib_pretty_uint_(mulib_pretty_parser_ *p,size_t *n){
 mulib_pretty_ws_(p);if(*p->p<'0'||*p->p>'9')return 0;
 size_t v=0;
 while(*p->p>='0'&&*p->p<='9'){
  size_t digit=(size_t)(*p->p++-'0');if(v>(SIZE_MAX-digit)/10)return 0;v=v*10+digit;
 }
 *n=v;return 1;
}
static int mulib_pretty_style_(const char *s,mulib_pretty_style_t *out){
 static const char *const names[]={"normal","keyword","identifier","number","operator","comment","string","math-ident","math-number","math-operator"};
 for(size_t i=0;i<sizeof(names)/sizeof(*names);i++)if(!strcmp(names[i],s)){*out=(mulib_pretty_style_t)i;return 1;}
 return 0;
}
static mulib_pretty_id_t mulib_pretty_parse_node_(mulib_pretty_parser_ *p,unsigned depth){
 if(depth>128 || ++p->nodes>200000 || !mulib_pretty_expect_(p,'('))return MULIB_PRETTY_NONE;
 char name[40];mulib_pretty_id_t a=MULIB_PRETTY_NONE,b=MULIB_PRETTY_NONE,result=MULIB_PRETTY_NONE;
 size_t number=0;mulib_pretty_style_t style=MULIB_PRETTY_NORMAL;
 if(!mulib_pretty_sym_(p,name,sizeof(name)))return MULIB_PRETTY_NONE;
 if(!strcmp(name,"text")||!strcmp(name,"fill")){
  char *s=mulib_pretty_quote_(p);if(!s)return MULIB_PRETTY_NONE;
  result=!strcmp(name,"text")?mulib_pretty_text(p->doc,s):mulib_pretty_fill(p->doc,s);
  free(s);
 }else if(!strcmp(name,"soft"))result=mulib_pretty_softline(p->doc);
 else if(!strcmp(name,"hard"))result=mulib_pretty_hardline(p->doc);
 else if(!strcmp(name,"nest")){
  if(!mulib_pretty_uint_(p,&number)||number>4096)return MULIB_PRETTY_NONE;
  a=mulib_pretty_parse_node_(p,depth+1);
  result=mulib_pretty_nest(p->doc,number,a);
 }else if(!strcmp(name,"annot")){
  char *s=mulib_pretty_quote_(p);if(!s)return MULIB_PRETTY_NONE;
  int ok=mulib_pretty_style_(s,&style);free(s);if(!ok)return MULIB_PRETTY_NONE;
  a=mulib_pretty_parse_node_(p,depth+1);
  result=mulib_pretty_annotate(p->doc,style,a);
 }else if(!strcmp(name,"region")||!strcmp(name,"paragraph")){
  char *s=mulib_pretty_quote_(p);if(!s)return MULIB_PRETTY_NONE;
  int which=-1;
  if(!strcmp(name,"region")){
   if(!strcmp(s,"greedy"))which=MULIB_PRETTY_GREEDY;
   else if(!strcmp(s,"fits"))which=MULIB_PRETTY_FITS;
   else if(!strcmp(s,"search"))which=MULIB_PRETTY_SEARCH;
  }else{
   if(!strcmp(s,"greedy"))which=MULIB_PRETTY_WORD_GREEDY;
   else if(!strcmp(s,"ragged"))which=MULIB_PRETTY_MIN_RAGGEDNESS;
  }
  free(s);if(which<0)return MULIB_PRETTY_NONE;
  a=mulib_pretty_parse_node_(p,depth+1);
  result=!strcmp(name,"region")?mulib_pretty_region(p->doc,(mulib_pretty_algorithm_t)which,a):
                             mulib_pretty_paragraph_region(p->doc,(mulib_pretty_paragraph_t)which,a);
 }else if(!strcmp(name,"cat")||!strcmp(name,"choice")){
  a=mulib_pretty_parse_node_(p,depth+1);
  b=mulib_pretty_parse_node_(p,depth+1);
  result=!strcmp(name,"cat")?mulib_pretty_cat(p->doc,a,b):mulib_pretty_choice(p->doc,a,b);
 }else if(!strcmp(name,"group")||!strcmp(name,"align")){
  a=mulib_pretty_parse_node_(p,depth+1);
  result=!strcmp(name,"group")?mulib_pretty_group(p->doc,a):mulib_pretty_align(p->doc,a);
 }
 if(result==MULIB_PRETTY_NONE||!mulib_pretty_expect_(p,')'))return MULIB_PRETTY_NONE;
 return result;
}
int mulib_pretty_parse_expr(const char *source,mulib_prettydoc_t **out,mulib_pretty_id_t *root){
 if(!source||!out||!root)return 0;
 size_t n=0;
 while(n<=2*1024*1024 && source[n])n++;
 if(n>2*1024*1024)return 0;
 mulib_pretty_parser_ p={0};p.p=source;p.doc=mulib_prettydoc_new();if(!p.doc)return 0;
 mulib_pretty_id_t r=mulib_pretty_parse_node_(&p,0);
 mulib_pretty_ws_(&p);
 if(r==MULIB_PRETTY_NONE||*p.p){mulib_prettydoc_free(p.doc);return 0;}
 *out=p.doc;*root=r;return 1;
}
char *mulib_pretty_render_expr(const char *source,const mulib_pretty_options_t *opt,mulib_pretty_target_t target){
 mulib_prettydoc_t *d=NULL;mulib_pretty_id_t root;char *result=NULL;
 if(mulib_pretty_parse_expr(source,&d,&root)){
  result=mulib_pretty_render(d,root,opt,target);mulib_prettydoc_free(d);
 }
 return result;
}
#ifdef __cplusplus
}
#endif
#endif
