/* Moving allocation/failure injection for owned notation trees and exporters. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef union notation_allocation {max_align_t alignment;struct{size_t size;uint64_t tag;} record;} notation_allocation;
static size_t calls,fail_at,live;
static void *notation_malloc(size_t n){notation_allocation *a;if(++calls==fail_at||n>SIZE_MAX-sizeof *a)return NULL;a=(notation_allocation *)malloc(sizeof *a+(n?n:1));if(!a)return NULL;a->record.size=n;a->record.tag=UINT64_C(0xabcd1234567890ef);++live;return a+1;}
static void notation_free(void *p){if(p){notation_allocation *a=(notation_allocation *)p-1;assert(a->record.tag==UINT64_C(0xabcd1234567890ef)&&live);a->record.tag=0;--live;free(a);}}
static void *notation_calloc(size_t n,size_t size){void *p;if(size&&n>SIZE_MAX/size)return NULL;p=notation_malloc(n*size);if(p)memset(p,0,n*size);return p;}
static void *notation_realloc(void *p,size_t n){void *next;size_t old;if(!p)return notation_malloc(n);if(!n){notation_free(p);return NULL;}old=((notation_allocation *)p-1)->record.size;next=notation_malloc(n);if(next){memcpy(next,p,n<old?n:old);notation_free(p);}return next;}
#define malloc notation_malloc
#define calloc notation_calloc
#define realloc notation_realloc
#define free notation_free
#include "../examples/notation-common.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
static void operation(unsigned kind){
    if(kind==0){const char s[]="| A | B |\n| --- | ---: |\n| x | y |";mulib_tbl_t *t=mulib_tbl_parse(s,sizeof s-1,NULL,NULL);if(t){char *out=mulib_tbl_render(t,"latex",NULL,NULL);notation_free(out);}mulib_xml_destroy(t);}
    else if(kind==1){const char s[]="sqrt(x^2)+frac(1e2,2)";mulib_eqn_t *n=mulib_eqn_parse(NULL,s,sizeof s-1,NULL);if(n){mulib_xml_node_t *m=mulib_eqn_mathml(n,0,NULL);char *out=mulib_eqn_latex(n,NULL,NULL);mulib_xml_destroy(m);notation_free(out);}mulib_eqn_destroy(n);}
    else if(kind==2){const char s[]="[Fe(CN)6]^3-";mulib_chem_formula_t *f=mulib_chem_parse(s,sizeof s-1,NULL);if(f){char *out=mulib_chem_latex(f,NULL,NULL);mulib_xml_node_t *m=mulib_chem_mathml(f,NULL);notation_free(out);mulib_xml_destroy(m);}mulib_chem_destroy(f);}
    else if(kind==3){mulib_symbol_registry_t *r=mulib_symbol_create();mulib_symbol_t symbol={"custom","★","\\star","mi",0,0};if(r)(void)mulib_symbol_define(r,&symbol);mulib_symbol_destroy(r);}
    else if(kind==4){mulib_chart_t *c=mulib_chart_create(NULL,NULL);mulib_chart_destroy(c);}
    else{mulib_paleography_t *c=mulib_paleography_create(NULL,NULL);mulib_paleography_destroy(c);}
}
static void exporter(unsigned kind) {
    mulib_paleography_t *p;
    mulib_paleography_glyph_t glyph={"triangle","M 0 0 Q 250 700 500 0 Z",0xe000,600};
    mulib_xml_node_t *node=NULL;char *text=NULL;size_t begin,offset=fail_at;
    fail_at=0;p=mulib_paleography_create(NULL,NULL);assert(p&&mulib_paleography_define(p,&glyph,NULL));
    begin=calls;if(offset)fail_at=calls+offset;
    if(kind==0)node=mulib_paleography_font(p,NULL);
    else if(kind==1)text=mulib_paleography_css(p,NULL,NULL);
    else if(kind==2)text=mulib_paleography_package(p,NULL,NULL);
    else if(kind==3)text=mulib_paleography_encoding(p,NULL,NULL);
    else if(kind==4)node=mulib_paleography_svg(p,"\xee\x80\x80",3,NULL,NULL,NULL);
    else text=mulib_paleography_latex(p,"\xee\x80\x80",3,NULL,NULL,NULL);
    offset=calls-begin;fail_at=0;notation_free(text);mulib_xml_destroy(node);mulib_paleography_destroy(p);assert(!live);
    /* During measurement report only exporter allocations, not preparation. */
    fail_at=offset;
}
int main(void){
    unsigned kind;size_t i,count,begin,samples;
    for(kind=0;kind<6;++kind){begin=calls;operation(kind);assert(!live);count=calls-begin;samples=kind>=4&&count>64?64:count;
        for(i=1;i<=samples;++i){fail_at=calls+(kind>=4?i*count/samples:i);operation(kind);fail_at=0;assert(!live);}}
    {mulib_paleography_t *p=mulib_paleography_create(NULL,NULL);mulib_paleography_glyph_t g={"triangle","M 0 0 L 500 0 L 250 700 Z",0xe000,600};size_t baseline=live;assert(p);
     for(i=1;i<=4;++i){fail_at=calls+i;assert(!mulib_paleography_define(p,&g,NULL));fail_at=0;assert(mulib_paleography_count(p)==0&&live==baseline);}mulib_paleography_destroy(p);assert(!live);}
    for(kind=0;kind<6;++kind) {
        exporter(kind);count=fail_at;fail_at=0;samples=kind>=4&&count>96?96:count;
        for(i=1;i<=samples;++i){fail_at=kind>=4?i*count/samples:i;exporter(kind);fail_at=0;}
    }
    return 0;
}
