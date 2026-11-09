#define MULIB_IMPL_PANDOCAST
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_XML
#define MULIB_IMPL_JSON
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_GLOB
#include "mulib-pandocast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    mulib_dsdom_t *doc = mulib_dsdom_element("docstril",NULL);
    mulib_dsdom_t *paragraph = mulib_dsdom_element("paragraph","Hello, Pandoc!");
    char *json;
    assert(doc && paragraph && mulib_xml_set_attr(doc,"version","1") && mulib_xml_append(doc,paragraph));
    json = mulib_pandocast_serialize(doc,NULL,NULL); assert(json); puts(json);
    free(json); mulib_xml_destroy(doc); return 0;
}
