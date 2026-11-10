#define MULIB_IMPL_COMPILER_CACHE
#include "mulib-compiler-cache.h"
#include <assert.h>
int main(void) {
    mulib_compiler_cache_t cache={0};const unsigned char artifact[]={0,1,0,2};
    const void *data;size_t length;
    assert(mulib_compiler_cache_put(&cache,123,artifact,sizeof artifact));
    assert(mulib_compiler_cache_get(&cache,123,&data,&length));
    assert(length==sizeof artifact&&!memcmp(data,artifact,length));
    mulib_compiler_cache_free(&cache);return 0;
}
