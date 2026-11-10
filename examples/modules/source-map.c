#define MULIB_IMPL_SOURCE_MAP
#include "mulib-source-map.h"
#include <assert.h>
int main(void) {
    mulib_source_map_t map={0};mulib_source_map_entry_t location;
    assert(mulib_source_map_add(&map,10,5,2,100));
    assert(mulib_source_map_find(&map,12,&location)&&location.original==102&&location.source_id==2);
    mulib_source_map_free(&map);return 0;
}
