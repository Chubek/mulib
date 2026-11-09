#define MULIB_IMPL_TAS
#define MULIB_IMPL_XTAS
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-tas.h"
#include <assert.h>
int main(void) {
    mulib_dsdom_t *node = mulib_dsdom_element("paragraph","text"), *copy;
    mulib_tas_t *tokens = mulib_tas_from_dom(node);
    assert(tokens && mulib_tas_count(tokens) == 3);
    copy = mulib_tas_to_dom(tokens,NULL); assert(copy);
    mulib_xml_destroy(copy); mulib_tas_destroy(tokens); mulib_xml_destroy(node); return 0;
}
