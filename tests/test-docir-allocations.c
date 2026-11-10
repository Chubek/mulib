#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
static long remaining_=-1,live_=0;
static int permit_(void){if(!remaining_)return 0;if(remaining_>0)remaining_--;return 1;}
static void *allocate_(size_t n){if(!permit_())return NULL;void *p=malloc(n);if(p)live_++;return p;}
static void *zero_(size_t n,size_t size){if(size&&n>SIZE_MAX/size)return NULL;void *p=allocate_(n*size);if(p)memset(p,0,n*size);return p;}
static void *resize_(void *old,size_t n){if(!permit_())return NULL;int fresh=old==NULL;void *p=realloc(old,n);if(p&&fresh)live_++;return p;}
static void release_(void *p){if(p){live_--;free(p);}}
#define malloc allocate_
#define calloc zero_
#define realloc resize_
#define free release_
#define MULIB_ENABLE_DOCIR
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCIR
#define MULIB_IMPL_DOCVALIDATE
#define MULIB_IMPL_DOCNORMALIZE
#define MULIB_IMPL_DOCSERIALIZE
#define MULIB_IMPL_TYPESRC
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TEX
#define MULIB_IMPL_LATEX
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_ZIP
#define MULIB_IMPL_EPUB
#include "mulib-docvalidate.h"
#include "mulib-docnormalize.h"
#include "mulib-docserialize.h"
#include "mulib-typesrc.h"
#include "mulib-latex.h"
#include "mulib-epub.h"
#include "fixtures/docir-document.h"

int main(void) {
    mulib_typesrc_source_t source={0};remaining_=-1;assert(mulib_typesrc_init(&source,"before",6));
    char *old=source.data;remaining_=0;
    assert(!mulib_typesrc_assign(&source,"after",5)&&source.data==old&&!strcmp(source.data,"before"));
    remaining_=-1;assert(mulib_typesrc_assign(&source,source.data+2,4)&&!strcmp(source.data,"fore"));
    mulib_typesrc_destroy(&source);assert(live_==0);
    int success=0;
    for(long limit=0;limit<2048&&!success;limit++) {
        remaining_=limit;mulib_docir_node_t *root=mulib_test_docir_document_();success=root!=NULL;
        mulib_docir_destroy(root);assert(live_==0);
    }
    assert(success);remaining_=-1;mulib_docir_node_t *root=mulib_test_docir_document_();assert(root);
    long baseline=live_;mulib_typecore_buf_t encoded={0};assert(mulib_docserialize_tree(root,NULL,&encoded,NULL));
    long encoded_baseline=live_;
    for(unsigned operation=0;operation<7;operation++) {
        success=0;
        for(long limit=0;limit<2048&&!success;limit++) {
            remaining_=-1;mulib_typecore_buf_t bytes={0};mulib_strbuf_t html={0};mulib_latex_t latex={0};
            mulib_buffer_t epub={0};mulib_docir_node_t *copy=NULL;
            if(operation==0||operation==1||operation==2)assert(mulib_typecore_text(&bytes,"prefix"));
            remaining_=limit;
            switch(operation) {
            case 0:success=mulib_docir_html(root,&bytes);break;
            case 1:success=mulib_docir_latex(root,&bytes);break;
            case 2:success=mulib_docserialize_tree(root,NULL,&bytes,NULL);break;
            case 3:success=mulib_docserialize_read(encoded.data,encoded.size,NULL,&copy,NULL);break;
            case 4:copy=mulib_docnormalize_tree(root,NULL,NULL);success=copy!=NULL;break;
            case 5:success=mulib_latex_from_docir(&latex,"Title",root);break;
            default: {
                mulib_epub_book_t book={"urn:example:faults","Faults","en","MuLIB","2026-11-10T00:00:00Z",NULL,0};
                mulib_epub_docir_chapter_t chapter={"chapter.xhtml","Title",root};
                success=mulib_epub_write_docir(&book,&chapter,1,&epub)==0;break;
            }
            }
            if(operation<3)assert(!memcmp(bytes.data,"prefix",6)&&(success||bytes.size==6));
            if(!success)assert(!copy&&!latex.source.data&&!latex.state&&!epub.data&&!epub.length);
            mulib_docir_destroy(copy);mulib_typecore_free(&bytes);mulib_strbuf_free(&html);
            mulib_latex_free(&latex);mulib_buffer_free(&epub);assert(live_==encoded_baseline);
        }
        assert(success);
    }
    remaining_=-1;mulib_typecore_free(&encoded);assert(live_==baseline);mulib_docir_destroy(root);assert(live_==0);
    return 0;
}
