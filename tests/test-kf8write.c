/* Standalone API contract test for mulib-kf8write. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_KF8WRITE
#include "mulib-kf8write.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    mulib_kf8write_flow_t f[]={ {0,10},{10,20} };
    mulib_typecore_buf_t b={0};
    assert(mulib_kf8write_fdst(f,2,&b));
    assert(b.size==28&&!memcmp(b.data,"FDST",4));
    size_t before=b.size;
    unsigned char original[28];memcpy(original,b.data,sizeof(original));
    assert(!mulib_kf8write_fdst(f,(size_t)((UINT32_MAX-12u)/8u)+1,&b));
    assert(!mulib_kf8write_fdst(f,SIZE_MAX,&b));
    assert(!mulib_kf8write_fdst(NULL,1,&b));
    assert(!mulib_kf8write_fdst(f,2,NULL));
    mulib_kf8write_flow_t overlap[]={ {0,10},{9,1} };
    mulib_kf8write_flow_t overflow[]={ {UINT32_MAX,1} };
    assert(!mulib_kf8write_fdst(overlap,2,&b));
    assert(!mulib_kf8write_fdst(overflow,1,&b));
    assert(b.size==before&&!memcmp(b.data,original,sizeof(original)));
    mulib_typecore_free(&b);
    assert(mulib_kf8write_fdst(NULL,0,&b));
    assert(b.size==12);
    mulib_typecore_free(&b);
    return 0;
}
