/* Native-only DocIR deserialization, ownership corruption and atomic export. */
#define MULIB_ENABLE_DOCIR
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCIR
#define MULIB_IMPL_DOCSERIALIZE
#include "mulib-docserialize.h"
#include <assert.h>

int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size) {
    if(size>65536)return 0;
    mulib_docir_limits_t limits={256,65536,64};mulib_docir_node_t *tree=NULL;
    if(mulib_docserialize_read(data,size,&limits,&tree,NULL)) {
        mulib_typecore_buf_t encoded={0},html={0},latex={0};
        assert(mulib_docserialize_tree(tree,&limits,&encoded,NULL));
        assert(encoded.size==size&&!memcmp(encoded.data,data,size));
        (void)mulib_docir_html(tree,&html);(void)mulib_docir_latex(tree,&latex);
        mulib_typecore_free(&encoded);mulib_typecore_free(&html);mulib_typecore_free(&latex);mulib_docir_destroy(tree);
    }
    mulib_docir_node_t *nodes[16]={0};size_t count=1;
    nodes[0]=mulib_docir_new(mulib_docir_ROOT,NULL);if(!nodes[0])return 0;
    for(size_t at=0;at+2<size&&at<384;at+=3) {
        size_t a=data[at+1]%count,b=data[at+2]%count;
        switch(data[at]%8) {
        case 0:if(count<16){nodes[count]=mulib_docir_new(data[at+1]%24,"text");if(nodes[count])count++;}break;
        case 1:(void)mulib_docir_append(nodes[a],nodes[b]);break;
        case 2:if(nodes[a]->count&&nodes[a]->children)nodes[a]->children[0]=nodes[b];break;
        case 3:nodes[a]->count=SIZE_MAX;break;
        case 4:(void)mulib_docir_set_text(nodes[a],"<&> \\input{x}");break;
        case 5:(void)mulib_docir_set_target(nodes[a],data[at+1]&1?"#missing":"javascript:bad");break;
        case 6:(void)mulib_docir_detach(nodes[a],0);break;
        default:nodes[a]->kind=1000;break;
        }
    }
    mulib_typecore_buf_t out={0};assert(mulib_typecore_text(&out,"prefix"));
    int ok=mulib_docir_html(nodes[0],&out);
    assert(!memcmp(out.data,"prefix",6)&&(ok||out.size==6));mulib_typecore_free(&out);
    int roots[16]={0};for(size_t i=0;i<count;i++)roots[i]=mulib_docir_parent(nodes[i])==NULL;
    for(size_t i=0;i<count;i++)if(roots[i])mulib_docir_destroy(nodes[i]);
    return 0;
}
