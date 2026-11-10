#define MULIB_IMPL_SOURCE_MANAGER
#include "mulib-source-manager.h"
#include <assert.h>
int main(void) {
    mulib_source_manager_t source={0};size_t line,column;
    assert(mulib_source_manager_set(&source,"hello\nworld",11));
    assert(mulib_source_manager_position(&source,7,&line,&column)&&line==2&&column==2);
    mulib_source_manager_free(&source);return 0;
}
