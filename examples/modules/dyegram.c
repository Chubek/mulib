#define MULIB_IMPL_DYEGRAM
#define MULIB_IMPL_GVIZZ
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-dyegram.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    const char source[] = "Client -> Server: Request\nServer -> Client: Reply\n";
    mulib_gvizz_t *messages = mulib_gvizz_parse(source,sizeof source-1,NULL);
    mulib_dsdom_t *svg;
    char *text;
    assert(messages);
    svg = mulib_dyegram_svg(messages); assert(svg);
    text = mulib_xml_serialize(svg,NULL); assert(text); puts(text);
    free(text); mulib_xml_destroy(svg); mulib_gvizz_destroy(messages); return 0;
}
