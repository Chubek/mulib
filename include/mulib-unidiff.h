/* MuLIB exact unified-diff parser and patch applicator, C11 single-header.
 * Define MULIB_IMPL_UNIDIFF in exactly one TU. Binary-safe text payloads,
 * bounded memory, no shell execution, no fuzzy matching, no path traversal.
 * Supports standard ---/+++, @@ ranges, /dev/null creates/deletes, and
 * '\\ No newline at end of file'. Rejects rename/copy/binary/combined diffs.
 * Hunk byte data is borrowed from patch->source; do not shallow-copy patch.
 */
#ifndef MULIB_UNIDIFF_H
#define MULIB_UNIDIFF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_ud_status { MULIB_UD_OK=0, MULIB_UD_INVALID=-1,
    MULIB_UD_LIMIT=-2, MULIB_UD_NOMEM=-3, MULIB_UD_CONFLICT=-4,
    MULIB_UD_UNSUPPORTED=-5 } mulib_ud_status_t;
typedef struct mulib_ud_line { char op; const unsigned char *text;
    size_t length; int newline; } mulib_ud_line_t;
typedef struct mulib_ud_hunk { size_t old_start,old_count,new_start,new_count;
    mulib_ud_line_t *lines; size_t line_count; } mulib_ud_hunk_t;
typedef struct mulib_ud_file {char *old_path,*new_path;
    mulib_ud_hunk_t *hunks; size_t hunk_count;} mulib_ud_file_t;
typedef struct mulib_ud_patch {unsigned char *source;size_t source_length;
    mulib_ud_file_t *files;size_t file_count;} mulib_ud_patch_t;
typedef struct mulib_ud_limits {size_t max_patch_bytes,max_files,max_hunks,
    max_lines,max_output_bytes;} mulib_ud_limits_t;
typedef struct mulib_ud_error {size_t line,file,hunk;int code;} mulib_ud_error_t;
typedef struct mulib_ud_blob {unsigned char *data;size_t length;} mulib_ud_blob_t;
void mulib_ud_init(mulib_ud_patch_t *patch);
void mulib_ud_free(mulib_ud_patch_t *patch);
void mulib_ud_blob_free(mulib_ud_blob_t *blob);
int mulib_ud_parse(const void *text,size_t length,const mulib_ud_limits_t *limits,
    mulib_ud_patch_t *out,mulib_ud_error_t *error);
/* Apply a file patch to a precise base snapshot. NULL/0 is a valid empty base.
 * For delete file, output must be empty. Output untouched on failure. */
int mulib_ud_apply(const mulib_ud_file_t *file,const void *base,size_t base_len,
    size_t max_output_bytes,mulib_ud_blob_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_UNIDIFF) && !defined(MULIB_UNIDIFF_IMPLEMENTED)
#define MULIB_UNIDIFF_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_ud_cursor_ {unsigned char *buf;size_t len,off,line;} mulib_ud_cursor_;
typedef struct mulib_ud_view_ {const unsigned char *p;size_t n;int has_lf;} mulib_ud_view_;
static int mulib_ud_next_(mulib_ud_cursor_ *c,mulib_ud_view_ *v) {
    if(c->off>=c->len)return 0;
    size_t i=c->off;while(i<c->len && c->buf[i]!='\n')i++;
    v->p=c->buf+c->off;v->n=i-c->off;v->has_lf=i<c->len;
    c->off=i+(i<c->len);c->line++;return 1;
}
static int mulib_ud_peek_(mulib_ud_cursor_ *c,mulib_ud_view_ *v) {
    mulib_ud_cursor_ tmp=*c;return mulib_ud_next_(&tmp,v);
}
static int mulib_ud_prefix_(mulib_ud_view_ v,const char *prefix) {
    size_t n=strlen(prefix);return v.n>=n&&!memcmp(v.p,prefix,n);
}
static int mulib_ud_lineeq_(mulib_ud_view_ v,const char *s){
    size_t n=strlen(s);return (v.n==n && !memcmp(v.p,s,n)) ||
        (v.n==n+1&&v.p[n]=='\r'&&!memcmp(v.p,s,n));
}
static int mulib_ud_digits_(const unsigned char **p,const unsigned char *end,size_t *v){
    if(*p>=end || **p<'0' || **p>'9')return 0;
    size_t n=0;
    while(*p<end&&**p>='0'&&**p<='9'){
        size_t d=(size_t)(**p-'0');if(n>(SIZE_MAX-d)/10)return 0;
        n=n*10+d;(*p)++;
    }*v=n;return 1;
}
static int mulib_ud_header_(mulib_ud_view_ v,mulib_ud_hunk_t *h) {
    const unsigned char *p=v.p,*end=p+v.n;
    if(v.n<8 || memcmp(p,"@@ -",4))return 0;
    p+=4;
    if(!mulib_ud_digits_(&p,end,&h->old_start))return 0;
    h->old_count=1;
    if(p<end&&*p==','){p++;if(!mulib_ud_digits_(&p,end,&h->old_count))return 0;}
    if((size_t)(end-p)<2||p[0]!=' '||p[1]!='+')return 0;
    p+=2;
    if(!mulib_ud_digits_(&p,end,&h->new_start))return 0;
    h->new_count=1;
    if(p<end&&*p==','){p++;if(!mulib_ud_digits_(&p,end,&h->new_count))return 0;}
    if((size_t)(end-p)<3||memcmp(p," @@",3))return 0;
    if(h->old_count&&h->old_start==0)return 0;
    if(h->new_count&&h->new_start==0)return 0;
    return 1;
}
static char *mulib_ud_path_(mulib_ud_view_ v,const char *prefix) {
    if(!mulib_ud_prefix_(v,prefix))return NULL;
    const unsigned char *p=v.p+strlen(prefix),*end=v.p+v.n;
    if(end>p && end[-1]=='\r')end--;
    /* A tab is ambiguous without a supported filename/timestamp grammar.
     * Never silently select the prefix before it as the target. */
    if(p==end || *p=='"' || memchr(p,'\t',(size_t)(end-p)) ||
       memchr(p,'\r',(size_t)(end-p)))return NULL;
    if((size_t)(end-p)==9 && !memcmp(p,"/dev/null",9)) {
        char *empty=malloc(1);if(empty)empty[0]=0;return empty;
    }
    if(end-p>=2 && p[1]=='/' && (p[0]=='a'||p[0]=='b'))p+=2;
    size_t n=(size_t)(end-p);
    if(!n||n>4096||p[0]=='/'||p[n-1]=='/'||memchr(p,'\\',n)||memchr(p,'\0',n))return NULL;
    size_t beg=0;
    for(size_t i=0;i<=n;i++)if(i==n||p[i]=='/'){
        size_t z=i-beg;if(z==0||(z==1&&p[beg]=='.')||
            (z==2&&p[beg]=='.'&&p[beg+1]=='.'))return NULL;
        beg=i+1;
    }
    char *out=malloc(n+1);if(!out)return NULL;
    memcpy(out,p,n);out[n]=0;return out;
}
void mulib_ud_init(mulib_ud_patch_t *p){if(p)memset(p,0,sizeof *p);}
void mulib_ud_free(mulib_ud_patch_t *p){
    if(!p)return;
    for(size_t i=0;i<p->file_count;i++){
        mulib_ud_file_t *f=&p->files[i];
        free(f->old_path);free(f->new_path);
        for(size_t j=0;j<f->hunk_count;j++)free(f->hunks[j].lines);
        free(f->hunks);
    }
    free(p->files);free(p->source);memset(p,0,sizeof *p);
}
void mulib_ud_blob_free(mulib_ud_blob_t *b){if(b){free(b->data);b->data=NULL;b->length=0;}}
static int mulib_ud_reserve_(void **p,size_t *cap,size_t len,size_t size,size_t maximum){
    if(len>=maximum)return 0;
    if(len<*cap)return 1;
    size_t nc=*cap?*cap*2:4;
    if(nc<*cap||nc>maximum)nc=maximum;
    if(nc>SIZE_MAX/size)return 0;
    void *v=realloc(*p,nc*size);if(!v)return 0;
    *p=v;*cap=nc;return 1;
}
static mulib_ud_limits_t mulib_ud_limits_(const mulib_ud_limits_t *p){
    if(p)return *p;
    mulib_ud_limits_t l={16u*1024*1024,128,4096,500000,64u*1024*1024};return l;
}
int mulib_ud_parse(const void *text,size_t length,const mulib_ud_limits_t *options,
                   mulib_ud_patch_t *out,mulib_ud_error_t *error){
    mulib_ud_limits_t limits=mulib_ud_limits_(options);
    if(error)memset(error,0,sizeof *error);
    if(!out||(!text&&length)||!length||length>limits.max_patch_bytes||
       !limits.max_files||!limits.max_hunks||!limits.max_lines||length==SIZE_MAX)return MULIB_UD_INVALID;
    mulib_ud_patch_t patch={0};
    patch.source=malloc(length+1);if(!patch.source)return MULIB_UD_NOMEM;
    memcpy(patch.source,text,length);patch.source[length]=0;patch.source_length=length;
    mulib_ud_cursor_ c={patch.source,length,0,0};
    mulib_ud_view_ v;size_t fcap=0,total_hunks=0,total_lines=0;
    int rc=MULIB_UD_INVALID;
    while(mulib_ud_peek_(&c,&v)) {
        if(mulib_ud_prefix_(v,"diff --git ")||mulib_ud_prefix_(v,"index ")||
           mulib_ud_prefix_(v,"new file mode ")||mulib_ud_prefix_(v,"deleted file mode ")){
            mulib_ud_next_(&c,&v);continue;
        }
        if(mulib_ud_prefix_(v,"GIT binary patch")||mulib_ud_prefix_(v,"Binary files ")||
           mulib_ud_prefix_(v,"rename ")||mulib_ud_prefix_(v,"copy ")||
           mulib_ud_prefix_(v,"old mode ")||mulib_ud_prefix_(v,"new mode ")||
           mulib_ud_prefix_(v,"similarity index ")||mulib_ud_prefix_(v,"@@@")||
           mulib_ud_prefix_(v,"diff --cc ")||mulib_ud_prefix_(v,"diff --combined ")){
            rc=MULIB_UD_UNSUPPORTED;goto failed;
        }
        if(!mulib_ud_prefix_(v,"--- "))goto failed;
        if(!mulib_ud_reserve_((void**)&patch.files,&fcap,patch.file_count,sizeof(*patch.files),limits.max_files)){
            rc=MULIB_UD_LIMIT;goto failed;}
        mulib_ud_file_t *f=&patch.files[patch.file_count];memset(f,0,sizeof *f);
        patch.file_count++;
        mulib_ud_next_(&c,&v);
        f->old_path=mulib_ud_path_(v,"--- ");if(!f->old_path)goto failed;
        if(!mulib_ud_next_(&c,&v)||!mulib_ud_prefix_(v,"+++ "))goto failed;
        f->new_path=mulib_ud_path_(v,"+++ ");if(!f->new_path)goto failed;
        if(!*f->old_path && !*f->new_path)goto failed;
        if(*f->old_path&&*f->new_path&&strcmp(f->old_path,f->new_path)!=0){rc=MULIB_UD_UNSUPPORTED;goto failed;}
        size_t hcap=0;
        while(mulib_ud_peek_(&c,&v)&&mulib_ud_prefix_(v,"@@ ")){
            if(total_hunks>=limits.max_hunks){rc=MULIB_UD_LIMIT;goto failed;}
            if(!mulib_ud_reserve_((void**)&f->hunks,&hcap,f->hunk_count,sizeof(*f->hunks),limits.max_hunks)){
                rc=MULIB_UD_NOMEM;goto failed;}
            mulib_ud_hunk_t *h=&f->hunks[f->hunk_count];memset(h,0,sizeof *h);
            f->hunk_count++;total_hunks++;
            mulib_ud_next_(&c,&v);
            if(!mulib_ud_header_(v,h))goto failed;
            if(h->old_count>limits.max_lines||h->new_count>limits.max_lines||
               h->old_count>SIZE_MAX-h->new_count){rc=MULIB_UD_LIMIT;goto failed;}
            size_t oc=0,nc=0,lcap=0;
            while(oc<h->old_count||nc<h->new_count) {
                if(!mulib_ud_next_(&c,&v)||v.n==0){goto failed;}
                if(v.p[0]!=' '&&v.p[0]!='+'&&v.p[0]!='-')goto failed;
                if(total_lines>=limits.max_lines){rc=MULIB_UD_LIMIT;goto failed;}
                if(!mulib_ud_reserve_((void**)&h->lines,&lcap,h->line_count,sizeof(*h->lines),limits.max_lines)){
                    rc=MULIB_UD_NOMEM;goto failed;}
                mulib_ud_line_t *l=&h->lines[h->line_count++];total_lines++;
                l->op=(char)v.p[0];l->text=v.p+1;l->length=v.n-1;l->newline=v.has_lf;
                if(l->op!='+')oc++;
                if(l->op!='-')nc++;
                if(oc>h->old_count||nc>h->new_count)goto failed;
                if(mulib_ud_peek_(&c,&v)&&mulib_ud_lineeq_(v,"\\ No newline at end of file")){
                    mulib_ud_next_(&c,&v);l->newline=0;
                }
            }
        }
        if(!f->hunk_count)goto failed;
    }
    if(!patch.file_count)goto failed;
    *out=patch;return MULIB_UD_OK;
failed:
    if(error){error->line=c.line+1;error->file=patch.file_count;error->hunk=patch.file_count?patch.files[patch.file_count-1].hunk_count:0;error->code=rc;}
    mulib_ud_free(&patch);return rc;
}

typedef struct mulib_ud_builder_ {unsigned char *p;size_t n,cap,limit;} mulib_ud_builder_;
static int mulib_ud_append_(mulib_ud_builder_ *b,const unsigned char *p,size_t n){
    if(n>b->limit-b->n)return MULIB_UD_LIMIT;
    if(n && b->n+n>b->cap){size_t need=b->n+n,cap=b->cap?b->cap:64;
        while(cap<need){if(cap>b->limit/2){cap=b->limit;break;}cap*=2;}
        if(cap<need)return MULIB_UD_LIMIT;
        unsigned char *q=realloc(b->p,cap+1);if(!q)return MULIB_UD_NOMEM;
        b->p=q;b->cap=cap;}
    if(n)memcpy(b->p+b->n,p,n);
    b->n+=n;return MULIB_UD_OK;
}
static int mulib_ud_source_line_(const unsigned char *src,size_t len,size_t *off,
                                 const unsigned char **p,size_t *n,int *newline){
    if(*off>=len)return 0;
    size_t start=*off,pos=start;while(pos<len&&src[pos]!='\n')pos++;
    *p=src+start;*n=pos-start;*newline=pos<len;
    *off=pos+(pos<len);return 1;
}
static int mulib_ud_take_(const unsigned char *src,size_t len,size_t *off,
    size_t *line_no,mulib_ud_builder_ *b,const mulib_ud_line_t *expected){
    const unsigned char *p;size_t n;int newline;
    if(!mulib_ud_source_line_(src,len,off,&p,&n,&newline))return MULIB_UD_CONFLICT;
    if(expected&&(n!=expected->length||newline!=expected->newline||
                 memcmp(p,expected->text,n)!=0))return MULIB_UD_CONFLICT;
    (*line_no)++;
    return mulib_ud_append_(b,p,n+((size_t)newline));
}
int mulib_ud_apply(const mulib_ud_file_t *f,const void *base,size_t len,
                   size_t max_out,mulib_ud_blob_t *out){
    if(!f||!out||(!base&&len)||!max_out||max_out==SIZE_MAX||!f->hunk_count)return MULIB_UD_INVALID;
    if(!*f->old_path && len)return MULIB_UD_CONFLICT;
    const unsigned char *src=(const unsigned char*)base;size_t offset=0,line=0,new_line=0;
    mulib_ud_builder_ b={NULL,0,0,max_out};int rc=MULIB_UD_OK;
    for(size_t i=0;i<f->hunk_count;i++){
        const mulib_ud_hunk_t *h=&f->hunks[i];
        size_t old_target=h->old_count?h->old_start-1:h->old_start;
        size_t new_target=h->new_count?h->new_start-1:h->new_start;
        if(old_target<line||new_target<new_line){rc=MULIB_UD_CONFLICT;goto fail;}
        while(line<old_target){rc=mulib_ud_take_(src,len,&offset,&line,&b,NULL);
            if(rc)goto fail;
            new_line++;}
        if(new_line!=new_target){rc=MULIB_UD_CONFLICT;goto fail;}
        size_t oc=0,nc=0;
        for(size_t j=0;j<h->line_count;j++){
            const mulib_ud_line_t *l=&h->lines[j];
            if(l->op==' '||l->op=='-'){
                /* Deletions still compare bytes but do not add output. */
                if(l->op==' '){rc=mulib_ud_take_(src,len,&offset,&line,&b,l);
                    if(rc)goto fail;
            new_line++;}
                else {const unsigned char *p;size_t n;int newline;
                    if(!mulib_ud_source_line_(src,len,&offset,&p,&n,&newline)||
                       n!=l->length||newline!=l->newline||memcmp(p,l->text,n)){
                        rc=MULIB_UD_CONFLICT;goto fail;}
                    line++;}
                oc++;
            }
            if(l->op=='+'){
                rc=mulib_ud_append_(&b,l->text,l->length);if(rc)goto fail;
                if(l->newline){static const unsigned char nl='\n';rc=mulib_ud_append_(&b,&nl,1);if(rc)goto fail;}
                new_line++;
            }
            if(l->op!='-')nc++;
        }
        if(oc!=h->old_count||nc!=h->new_count){rc=MULIB_UD_INVALID;goto fail;}
    }
    while(offset<len){rc=mulib_ud_take_(src,len,&offset,&line,&b,NULL);if(rc)goto fail;}
    if(!*f->new_path && b.n){rc=MULIB_UD_CONFLICT;goto fail;}
    if(!b.p){b.p=malloc(1);if(!b.p){rc=MULIB_UD_NOMEM;goto fail;}}
    b.p[b.n]=0;out->data=b.p;out->length=b.n;return MULIB_UD_OK;
fail:free(b.p);return rc;
}
#ifdef __cplusplus
}
#endif
#endif
