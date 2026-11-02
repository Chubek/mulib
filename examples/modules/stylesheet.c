#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_STYLESHEET
#include "mulib-stylesheet.h"
#include <assert.h>
int main(void) {
    const char source[] = "text {font-size: 18px;} .title {color: #369;}";
    mulib_stylesheet_t *sheet = mulib_stylesheet_parse(source, sizeof source-1, NULL);
    mulib_xml_node_t *node = mulib_xml_new(MULIB_XML_ELEMENT, "text");
    mulib_stylesheet_style_t style;
    assert(sheet && node && mulib_xml_set_attr(node, "class", "title"));
    assert(!mulib_stylesheet_resolve(sheet, node, NULL, &style, NULL));
    assert(style.font_size == 18 && style.color == UINT32_C(0x336699ff));
    mulib_xml_destroy(node); mulib_stylesheet_destroy(sheet); return 0;
}
