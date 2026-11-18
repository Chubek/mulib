/**
 * @file mulib-prettyemit.h
 * @brief Plain, HTML5/MathML, LaTeX and troff source emitters for PrettyDoc layouts.
 * Define MULIB_IMPL_PRETTYEMIT in one C11 implementation translation unit.
 * Text is treated as untrusted and escaped for the target format.
 */
#ifndef MULIB_PRETTYEMIT_H
#define MULIB_PRETTYEMIT_H
#include "mulib-prettydoc.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_pretty_target {
 MULIB_PRETTY_PLAIN, MULIB_PRETTY_HTML_MATHML,
 MULIB_PRETTY_LATEX, MULIB_PRETTY_TROFF
} mulib_pretty_target_t;
/* malloc-owned string; NULL on failure or invalid arguments. */
char *mulib_pretty_emit(const mulib_pretty_layout_t *layout,mulib_pretty_target_t target);
char *mulib_pretty_render(const mulib_prettydoc_t *doc,mulib_pretty_id_t root,
  const mulib_pretty_options_t *options,mulib_pretty_target_t target);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PRETTYEMIT) && !defined(MULIB_PRETTYEMIT_IMPLEMENTED)
#define MULIB_PRETTYEMIT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_pretty_buf_ {char *s;size_t n,cap;} mulib_pretty_buf_;
static int mulib_pretty_append_(mulib_pretty_buf_ *b,const char *s,size_t n){
 if(!s || n>SIZE_MAX-b->n-1)return 0;
 size_t want=b->n+n+1;
 if(want>b->cap){size_t nc=b->cap?b->cap:128;
  while(nc<want){if(nc>SIZE_MAX/2){nc=want;break;}nc*=2;}
  char *v=(char*)realloc(b->s,nc);if(!v)return 0;b->s=v;b->cap=nc;}
 if(n)memcpy(b->s+b->n,s,n);
 b->n+=n;b->s[b->n]=0;return 1;
}
static int mulib_pretty_lit_(mulib_pretty_buf_ *b,const char *s){return mulib_pretty_append_(b,s,strlen(s));}
static int mulib_pretty_escape_html_(mulib_pretty_buf_ *b,const char *s,size_t n){
 for(size_t i=0;i<n;i++){
  const char *esc=s[i]=='&'?"&amp;":s[i]=='<'?"&lt;":s[i]=='>'?"&gt;":
           s[i]=='"'?"&quot;":s[i]=='\''?"&#39;":NULL;
  if(!(esc?mulib_pretty_lit_(b,esc):mulib_pretty_append_(b,s+i,1)))return 0;
 }
 return 1;
}
static int mulib_pretty_escape_tex_(mulib_pretty_buf_ *b,const char *s,size_t n){
 for(size_t i=0;i<n;i++){
  const char *esc=NULL;
  switch(s[i]){
   case '\\':esc="\\textbackslash{}";break;
   case '{':esc="\\{";break;case '}':esc="\\}";break;
   case '$':esc="\\$";break;case '&':esc="\\&";break;
   case '%':esc="\\%";break;case '#':esc="\\#";break;
   case '_':esc="\\_";break;case '^':esc="\\textasciicircum{}";break;
   case '~':esc="\\textasciitilde{}";break;
   case ' ':esc="\\ ";break;
   case '\n':esc="\\par\\noindent\n";break;
  }
  if(!(esc?mulib_pretty_lit_(b,esc):mulib_pretty_append_(b,s+i,1)))return 0;
 }
 return 1;
}
static int mulib_pretty_escape_roff_(mulib_pretty_buf_ *b,const char *s,size_t n,int *line_start){
 for(size_t i=0;i<n;i++){
  if(*line_start && (s[i]=='.'||s[i]=='\''))if(!mulib_pretty_lit_(b,"\\&"))return 0;
  if(s[i]=='\\'){if(!mulib_pretty_lit_(b,"\\e"))return 0;}
  else if(!mulib_pretty_append_(b,s+i,1))return 0;
  *line_start=s[i]=='\n';
 }
 return 1;
}
static const char *mulib_pretty_class_(mulib_pretty_style_t s){
 static const char *const names[]={"text","keyword","identifier","number","operator","comment","string","math-ident","math-number","math-operator"};
 return (unsigned)s<sizeof(names)/sizeof(*names)?names[ (unsigned)s ]:NULL;
}
char *mulib_pretty_emit(const mulib_pretty_layout_t *l,mulib_pretty_target_t target){
 if(!l || !l->text || target<0 || target>MULIB_PRETTY_TROFF)return NULL;
 mulib_pretty_buf_ b={0};int ok=1;int line_start=1;
 if(target==MULIB_PRETTY_PLAIN)ok=mulib_pretty_append_(&b,l->text,l->length);
 else if(target==MULIB_PRETTY_HTML_MATHML)ok=mulib_pretty_lit_(&b,"<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>Formatted code</title></head>\n<body><pre class=\"mulib-pretty\"><code>");
 else if(target==MULIB_PRETTY_LATEX)ok=mulib_pretty_lit_(&b,"\\documentclass{article}\n\\usepackage[T1]{fontenc}\n\\begin{document}\n\\noindent{\\ttfamily ");
 else if(target==MULIB_PRETTY_TROFF)ok=mulib_pretty_lit_(&b,".nf\n.ft CW\n");
 if(!ok){free(b.s);return NULL;}
 if(target!=MULIB_PRETTY_PLAIN){
  for(size_t i=0;i<l->count && ok;i++){
   const mulib_pretty_piece_t *p=&l->pieces[i];
   if(p->start>l->length || p->length>l->length-p->start){ok=0;break;}
   const char *s=l->text+p->start;size_t n=p->length;
   if(target==MULIB_PRETTY_HTML_MATHML){
    const char *cls=mulib_pretty_class_(p->style);if(!cls){ok=0;break;}
    if(p->style>=MULIB_PRETTY_MATH_IDENT){
     const char *tag=p->style==MULIB_PRETTY_MATH_IDENT?"mi":p->style==MULIB_PRETTY_MATH_NUMBER?"mn":"mo";
     ok=mulib_pretty_lit_(&b,"<math xmlns=\"http://www.w3.org/1998/Math/MathML\"><")&&
        mulib_pretty_lit_(&b,tag)&&mulib_pretty_lit_(&b,">")&&
        mulib_pretty_escape_html_(&b,s,n)&&mulib_pretty_lit_(&b,"</")&&
        mulib_pretty_lit_(&b,tag)&&mulib_pretty_lit_(&b,"></math>");
    }else if(p->style!=MULIB_PRETTY_NORMAL){
     ok=mulib_pretty_lit_(&b,"<span class=\"tok-")&&mulib_pretty_lit_(&b,cls)&&
        mulib_pretty_lit_(&b,"\">")&&mulib_pretty_escape_html_(&b,s,n)&&mulib_pretty_lit_(&b,"</span>");
    }else ok=mulib_pretty_escape_html_(&b,s,n);
   }else if(target==MULIB_PRETTY_LATEX){
    if(p->style==MULIB_PRETTY_KEYWORD)ok=mulib_pretty_lit_(&b,"\\textbf{");
    else if(p->style==MULIB_PRETTY_COMMENT)ok=mulib_pretty_lit_(&b,"\\textit{");
    if(ok)ok=mulib_pretty_escape_tex_(&b,s,n);
    if(ok && (p->style==MULIB_PRETTY_KEYWORD||p->style==MULIB_PRETTY_COMMENT))ok=mulib_pretty_lit_(&b,"}");
   }else ok=mulib_pretty_escape_roff_(&b,s,n,&line_start);
  }
  if(ok){
   if(target==MULIB_PRETTY_HTML_MATHML)ok=mulib_pretty_lit_(&b,"</code></pre></body></html>\n");
   if(target==MULIB_PRETTY_LATEX)ok=mulib_pretty_lit_(&b,"}\n\\end{document}\n");
   if(target==MULIB_PRETTY_TROFF)ok=mulib_pretty_lit_(&b,"\n.ft\n.fi\n");
  }
 }
 if(!ok){free(b.s);return NULL;}
 if(!b.s){b.s=(char*)malloc(1);if(!b.s)return NULL;b.s[0]=0;}
 return b.s;
}
char *mulib_pretty_render(const mulib_prettydoc_t *d,mulib_pretty_id_t id,
 const mulib_pretty_options_t *opt,mulib_pretty_target_t target){
 mulib_pretty_layout_t l={0};char *result;
 if(!mulib_pretty_layout(d,id,opt,&l))return NULL;
 result=mulib_pretty_emit(&l,target);mulib_pretty_layout_free(&l);return result;
}
#ifdef __cplusplus
}
#endif
#endif
