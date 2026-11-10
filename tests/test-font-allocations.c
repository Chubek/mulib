#include <stdlib.h>
#include <string.h>
#include <assert.h>
static long remaining=-1,live=0;
static int permit_(void){if(remaining==0)return 0;if(remaining>0)remaining--;return 1;}
static void *allocate_(size_t n){if(!permit_())return NULL;void *p=malloc(n);if(p)live++;return p;}
static void *zero_(size_t n,size_t size){if(size&&n>(size_t)-1/size)return NULL;void *p=allocate_(n*size);if(p)memset(p,0,n*size);return p;}
static void *resize_(void *old,size_t n){if(!permit_())return NULL;int fresh=old==NULL;void *p=realloc(old,n);if(p&&fresh)live++;return p;}
static void release_(void *p){if(p){live--;free(p);}}
#define MULIB_TYPECORE_REALLOC resize_
#define MULIB_TYPECORE_FREE release_
#define MULIB_FONTSHAPE_MALLOC allocate_
#define MULIB_FONTSHAPE_CALLOC zero_
#define MULIB_FONTSHAPE_FREE release_
#define MULIB_FONTSHAPE_NATIVE_ONLY
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_SFNT
#define MULIB_IMPL_FONTCMAP
#define MULIB_IMPL_FONTGSUB
#define MULIB_IMPL_FONTGPOS
#define MULIB_IMPL_FONTGDEF
#define MULIB_IMPL_FONTMETRICS
#define MULIB_IMPL_FONTKERN
#define MULIB_IMPL_FONTSHAPE
#include "mulib-fontshape.h"
#include "fixtures/publishing-font.h"

int main(void) {
    unsigned char font[1024];size_t size=mulib_test_font_make_(font);int success=0;
    for(long limit=0;limit<8&&!success;limit++) {
        remaining=limit;mulib_fontshape_t *f=mulib_fontshape_open_memory(font,size,0,1000);
        success=f!=NULL;mulib_fontshape_close(f);assert(live==0);
    }
    assert(success);remaining=-1;
    mulib_fontshape_t *f=mulib_fontshape_open_memory(font,size,0,1000);assert(f);long baseline=live;
    success=0;
    for(long limit=0;limit<8&&!success;limit++) {
        mulib_fontshape_run_t run={0};remaining=limit;
        success=!mulib_fontshape_shape(f,"fiAV",4,NULL,&run);
        if(!success)assert(!run.glyphs&&!run.count);
        mulib_fontshape_run_destroy(&run);assert(live==baseline);
    }
    assert(success);remaining=-1;mulib_fontshape_close(f);assert(live==0);
    int (*escapes[])(mulib_typecore_buf_t*,const char*,size_t)={mulib_typecore_html,mulib_typecore_latex,mulib_typecore_json};
    for(size_t kind=0;kind<3;kind++) {
        char input[256];memset(input,kind==2?1:'&',sizeof(input));success=0;
        for(long limit=0;limit<16&&!success;limit++) {
            remaining=-1;mulib_typecore_buf_t b={0};assert(mulib_typecore_text(&b,"prefix"));
            remaining=limit;success=escapes[kind](&b,input,sizeof(input));
            assert(!memcmp(b.data,"prefix",6));if(!success)assert(b.size==6);
            mulib_typecore_free(&b);assert(live==0);
        }
        assert(success);
    }
    remaining=-1;mulib_typecore_buf_t b={0};char input[128];memset(input,'&',sizeof(input));
    assert(mulib_typecore_append(&b,input,sizeof(input)));
    assert(mulib_typecore_html(&b,(const char*)b.data,b.size));
    assert(b.size==768&&!memcmp(b.data+128,"&amp;",5));
    assert(!mulib_typecore_byte(&b,256)&&b.size==768);
    mulib_typecore_free(&b);assert(live==0);
    return 0;
}
