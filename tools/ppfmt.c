/* Portable source document renderer. Builds without other MuLIB kernels. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MULIB_IMPL_PRETTYDOC
#define MULIB_IMPL_PRETTYEMIT
#define MULIB_IMPL_PRETTYSEXPR
#include "mulib-prettysexpr.h"
static void usage(const char *program){
 fprintf(stderr,"usage: %s [--width N] [--ribbon N] [--algorithm greedy|fits|search]\n"
         "       [--paragraph greedy|ragged] [--target plain|html|latex|troff]\n"
         "       [-o FILE] [INPUT.ppdoc]\n",program);
}
static int read_width(const char *s,unsigned long min,size_t *out){
 unsigned long n=0;
 if(!*s)return 0;
 for(;*s;s++){
  if(*s<'0'||*s>'9')return 0;
  n=10*n+(unsigned long)(*s-'0');
  if(n>4096)return 0;
 }
 if(n<min)return 0;
 *out=(size_t)n;return 1;
}
static int read_all(FILE *f,char **out){
 size_t len=0,cap=4096;char *s=(char*)malloc(cap);if(!s)return 0;
 for(;;){
  if(len+1==cap){
   if(cap>=2*1024*1024+1){
    if(fgetc(f)!=EOF||ferror(f)){free(s);return 0;}
    break;
   }
   size_t nc=cap*2;if(nc>2*1024*1024+1)nc=2*1024*1024+1;
   char *t=(char*)realloc(s,nc);if(!t){free(s);return 0;}s=t;cap=nc;
  }
  size_t n=fread(s+len,1,cap-len-1,f);len+=n;
  if(n==0){if(ferror(f)){free(s);return 0;}break;}
 }
 s[len]=0;if(memchr(s,0,len)){free(s);return 0;}*out=s;return 1;
}
int main(int argc,char **argv){
 const char *input=NULL,*output=NULL;mulib_pretty_options_t opt;
 mulib_pretty_target_t target=MULIB_PRETTY_PLAIN;
 mulib_pretty_options_init(&opt);
 for(int i=1;i<argc;i++){
  const char *arg=argv[i];
  if(!strcmp(arg,"--help")){usage(argv[0]);return 0;}
  if(!strcmp(arg,"-o") && i+1<argc){output=argv[++i];continue;}
  if(!strcmp(arg,"--width")||!strcmp(arg,"--ribbon")){
   int ribbon=!strcmp(arg,"--ribbon");
   if(i+1>=argc||!read_width(argv[++i],ribbon?0:1,ribbon?&opt.ribbon_width:&opt.width)){
    usage(argv[0]);return 2;
   }
   continue;
  }
  if(!strcmp(arg,"--algorithm")&&i+1<argc){
   const char *s=argv[++i];
   if(!strcmp(s,"greedy"))opt.algorithm=MULIB_PRETTY_GREEDY;
   else if(!strcmp(s,"fits"))opt.algorithm=MULIB_PRETTY_FITS;
   else if(!strcmp(s,"search"))opt.algorithm=MULIB_PRETTY_SEARCH;
   else {usage(argv[0]);return 2;}continue;
  }
  if(!strcmp(arg,"--paragraph")&&i+1<argc){
   const char *s=argv[++i];
   if(!strcmp(s,"greedy"))opt.paragraph=MULIB_PRETTY_WORD_GREEDY;
   else if(!strcmp(s,"ragged"))opt.paragraph=MULIB_PRETTY_MIN_RAGGEDNESS;
   else {usage(argv[0]);return 2;}continue;
  }
  if(!strcmp(arg,"--target")&&i+1<argc){
   const char *s=argv[++i];
   if(!strcmp(s,"plain"))target=MULIB_PRETTY_PLAIN;
   else if(!strcmp(s,"html"))target=MULIB_PRETTY_HTML_MATHML;
   else if(!strcmp(s,"latex"))target=MULIB_PRETTY_LATEX;
   else if(!strcmp(s,"troff"))target=MULIB_PRETTY_TROFF;
   else {usage(argv[0]);return 2;}continue;
  }
  if(arg[0]=='-' && strcmp(arg,"-")){usage(argv[0]);return 2;}
  if(input){usage(argv[0]);return 2;}input=arg;
 }
 FILE *in=stdin;
 if(input && strcmp(input,"-")){in=fopen(input,"rb");if(!in){perror(input);return 1;}}
 char *spec=NULL;
 int ok=read_all(in,&spec);
 if(in!=stdin)fclose(in);
 if(!ok){fprintf(stderr,"ppfmt: input too large or unreadable\n");return 1;}
 char *result=mulib_pretty_render_expr(spec,&opt,target);free(spec);
 if(!result){fprintf(stderr,"ppfmt: malformed document, layout failure, or out of memory\n");return 1;}
 FILE *out=stdout;
 if(output){out=fopen(output,"wb");if(!out){perror(output);free(result);return 1;}}
 size_t count=strlen(result);
 ok=fwrite(result,1,count,out)==count;
 if(out!=stdout){if(fclose(out)!=0)ok=0;}else if(fflush(out)!=0)ok=0;
 free(result);
 if(!ok){fprintf(stderr,"ppfmt: output I/O failure\n");return 1;}
 return 0;
}
