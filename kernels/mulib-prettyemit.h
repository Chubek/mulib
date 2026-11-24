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
    case '\r':esc="\\textless{}CR\\textgreater{}";break;
    case '\t':esc="\\ \\ \\ \\ \\ \\ \\ \\ ";break;
    case '\177':esc="\\textless{}DEL\\textgreater{}";break;
  }
  if(!(esc?mulib_pretty_lit_(b,esc):mulib_pretty_append_(b,s+i,1)))return 0;
 }
 return 1;
}
static int mulib_pretty_escape_roff_(mulib_pretty_buf_ *b,const char *s,size_t n,int *line_start){
 for(size_t i=0;i<n;i++){
  if(*line_start && (s[i]=='.'||s[i]=='\''))if(!mulib_pretty_lit_(b,"\\&"))return 0;
   if(s[i]=='\\'){if(!mulib_pretty_lit_(b,"\\e"))return 0;}
   else if(s[i]=='\r'){if(!mulib_pretty_lit_(b,"<CR>"))return 0;}
   else if(s[i]=='\177'){if(!mulib_pretty_lit_(b,"<DEL>"))return 0;}
  else if(!mulib_pretty_append_(b,s+i,1))return 0;
  *line_start=s[i]=='\n';
 }
 return 1;
}
static const char *mulib_pretty_class_(mulib_pretty_style_t s){
 static const char *const names[]={"text","keyword","identifier","number","operator","comment","string","math-ident","math-number","math-operator"};
 return (unsigned)s<sizeof(names)/sizeof(*names)?names[ (unsigned)s ]:NULL;
}
static int mulib_pretty_eq_troff_atom_(mulib_pretty_buf_ *b,const char *s,size_t n){
 if(!mulib_pretty_lit_(b," \""))return 0;
 for(size_t i=0;i<n;i++){
  unsigned char c=(unsigned char)s[i];
  if(c<32||c=='"'||c=='\\'){
   /* Keep user input out of both eqn's lexer and roff's escape syntax. */
   char code[]="\\N'000'";
   code[3]=(char)('0'+c/100);code[4]=(char)('0'+c/10%10);code[5]=(char)('0'+c%10);
   if(!mulib_pretty_lit_(b,code))return 0;
  }else if(!mulib_pretty_append_(b,s+i,1))return 0;
 }
 return mulib_pretty_lit_(b,"\" ");
}
static int mulib_pretty_eq_troff_(mulib_pretty_buf_ *b,const mulib_pretty_layout_t *l,
 size_t *index,size_t limit,unsigned depth){
 if(depth>256||*index>=limit)return 0;
 const mulib_pretty_piece_t *p=&l->pieces[*index];
 switch(p->style){
 case MULIB_PRETTY_EQ_ATOM:
  return p->start<=l->length&&p->length<=l->length-p->start&&
         mulib_pretty_eq_troff_atom_(b,l->text+p->start,p->length);
 case MULIB_PRETTY_EQ_FRAC:case MULIB_PRETTY_EQ_SUP:case MULIB_PRETTY_EQ_SUB:
  return mulib_pretty_lit_(b," { { ");
 case MULIB_PRETTY_EQ_ROOT:return mulib_pretty_lit_(b," { sqrt { ");
 case MULIB_PRETTY_EQ_BETWEEN_FRAC:return mulib_pretty_lit_(b," } over { ");
 case MULIB_PRETTY_EQ_BETWEEN_SUP:return mulib_pretty_lit_(b," } sup { ");
 case MULIB_PRETTY_EQ_BETWEEN_SUB:return mulib_pretty_lit_(b," } sub { ");
 case MULIB_PRETTY_EQ_CLOSE_FRAC:case MULIB_PRETTY_EQ_CLOSE_SUP:
 case MULIB_PRETTY_EQ_CLOSE_SUB:case MULIB_PRETTY_EQ_CLOSE_ROOT:
  return mulib_pretty_lit_(b," } } ");
 case MULIB_PRETTY_EQ_MATRIX: {
  /* eqn matrices are column-major. Discover the row boundaries once, then
   * advance the row cursors independently while emitting each column. */
  size_t rows[32],ends[32],count=0,i=*index+1;
  while(i<limit && l->pieces[i].style==MULIB_PRETTY_EQ_ROW){
   if(count==32)return 0;
   rows[count]=++i;unsigned nested=0;
   while(i<limit){
    mulib_pretty_style_t s=l->pieces[i].style;
    if(!nested&&s==MULIB_PRETTY_EQ_CLOSE_ROW)break;
    if(s==MULIB_PRETTY_EQ_MATRIX)nested++;
    else if(s==MULIB_PRETTY_EQ_CLOSE_MATRIX){if(!nested)return 0;nested--;}
    ++i;
   }
   ends[count++]=i++;
  }
  if(!count||i>=limit||l->pieces[i].style!=MULIB_PRETTY_EQ_CLOSE_MATRIX||
     !mulib_pretty_lit_(b," matrix { "))return 0;
  *index=i;
  for(size_t col=0;col<32;col++){
   if(!mulib_pretty_lit_(b," lcol { "))return 0;
   for(size_t row=0;row<count;row++){
    if(rows[row]>=ends[row]||(row&&!mulib_pretty_lit_(b," above "))||
       !mulib_pretty_lit_(b," { "))return 0;
    while(rows[row]<ends[row]&&l->pieces[rows[row]].style!=MULIB_PRETTY_EQ_CELL){
     if(!mulib_pretty_eq_troff_(b,l,&rows[row],ends[row],depth+1))return 0;
     rows[row]++;
    }
    if(!mulib_pretty_lit_(b," } "))return 0;
   }
   if(!mulib_pretty_lit_(b," } "))return 0;
   if(rows[0]==ends[0]){
    for(size_t row=1;row<count;row++)if(rows[row]!=ends[row])return 0;
    return mulib_pretty_lit_(b," } ");
   }
   for(size_t row=0;row<count;row++){
    if(rows[row]>=ends[row])return 0;
    rows[row]++;
   }
  }
  return 0;
 }
 default:return 0;
 }
}
static int mulib_pretty_eq_markup_(mulib_pretty_buf_ *b,mulib_pretty_target_t target,
 mulib_pretty_style_t style,const char *text,size_t n){
 static const char *const html[]={
  "<math xmlns=\"http://www.w3.org/1998/Math/MathML\">","</math>",NULL,
  "<mfrac><mrow>","<msup><mrow>","<msub><mrow>","<msqrt><mrow>",
  "</mrow><mrow>","</mrow><mrow>","</mrow><mrow>",
  "</mrow></mfrac>","</mrow></msup>","</mrow></msub>","</mrow></msqrt>",
  "<mtable>","<mtr><mtd>","</mtd><mtd>","</mtd></mtr>","</mtable>"
 };
 static const char *const latex[]={
  "\\(","\\)",NULL,"\\frac{","{{","{{","\\sqrt{","}{","}^{","}_{",
  "}","}}","}}","}","\\begin{matrix}",""," & "," \\\\ ","\\end{matrix}"
 };
 size_t i=(size_t)(style-MULIB_PRETTY_EQ_BEGIN);
 if(i>=sizeof(html)/sizeof(*html))return 0;
 if(style==MULIB_PRETTY_EQ_ATOM)
  return target==MULIB_PRETTY_HTML_MATHML?
   mulib_pretty_lit_(b,"<mtext>")&&mulib_pretty_escape_html_(b,text,n)&&mulib_pretty_lit_(b,"</mtext>"):
   mulib_pretty_lit_(b,"\\text{")&&mulib_pretty_escape_tex_(b,text,n)&&mulib_pretty_lit_(b,"}");
 return mulib_pretty_lit_(b,target==MULIB_PRETTY_HTML_MATHML?html[i]:latex[i]);
}
char *mulib_pretty_emit(const mulib_pretty_layout_t *l,mulib_pretty_target_t target){
 if(!l || !l->text || (l->count&&!l->pieces) || target<0 || target>MULIB_PRETTY_TROFF)return NULL;
 mulib_pretty_buf_ b={0};int ok=1;int line_start=1;
 if(target==MULIB_PRETTY_PLAIN)ok=mulib_pretty_append_(&b,l->text,l->length);
 else if(target==MULIB_PRETTY_HTML_MATHML)ok=mulib_pretty_lit_(&b,"<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>Formatted code</title></head>\n<body><pre class=\"mulib-pretty\"><code>");
 else if(target==MULIB_PRETTY_LATEX){
  ok=mulib_pretty_lit_(&b,"\\documentclass{article}\n\\usepackage[T1]{fontenc}\n");
  for(size_t i=0;i<l->count&&ok;i++)if(l->pieces[i].style==MULIB_PRETTY_EQ_BEGIN){
   ok=mulib_pretty_lit_(&b,"\\usepackage{amsmath}\n");break;
  }
  if(ok)ok=mulib_pretty_lit_(&b,"\\begin{document}\n\\noindent{\\ttfamily ");
 }
  /* nroff's terminal output is already fixed-width and may not have a
   * Courier font. Groff's typesetting devices use CR (Courier Roman). */
  else if(target==MULIB_PRETTY_TROFF)ok=mulib_pretty_lit_(&b,".nf\n.if t .ft CR\n");
 if(!ok){free(b.s);return NULL;}
 if(target!=MULIB_PRETTY_PLAIN){
  int equation=0;
  for(size_t i=0;i<l->count && ok;i++){
   const mulib_pretty_piece_t *p=&l->pieces[i];
   if(p->start>l->length || p->length>l->length-p->start){ok=0;break;}
   const char *s=l->text+p->start;size_t n=p->length;
   if(p->style>=MULIB_PRETTY_EQ_BEGIN){
    if(p->style==MULIB_PRETTY_EQ_BEGIN){if(equation){ok=0;break;}equation=1;}
    else if(!equation){ok=0;break;}
    if(target==MULIB_PRETTY_TROFF){
     if(p->style==MULIB_PRETTY_EQ_BEGIN)ok=mulib_pretty_lit_(&b,line_start?".EQ\n":"\n.EQ\n");
     else if(p->style==MULIB_PRETTY_EQ_END){ok=mulib_pretty_lit_(&b,"\n.EN\n");line_start=1;}
     else ok=mulib_pretty_eq_troff_(&b,l,&i,l->count,0);
    }else ok=mulib_pretty_eq_markup_(&b,target,p->style,s,n);
    if(p->style==MULIB_PRETTY_EQ_END)equation=0;
    continue;
   }
   if(equation){ok=0;break;}
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
  if(equation)ok=0;
  if(ok){
   if(target==MULIB_PRETTY_HTML_MATHML)ok=mulib_pretty_lit_(&b,"</code></pre></body></html>\n");
   if(target==MULIB_PRETTY_LATEX)ok=mulib_pretty_lit_(&b,"}\n\\end{document}\n");
    if(target==MULIB_PRETTY_TROFF)ok=mulib_pretty_lit_(&b,"\n.if t .ft\n.fi\n");
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
