#define MULIB_IMPL_DOCSTRING
#define MULIB_IMPL_TBL
#define MULIB_IMPL_CSV
#define MULIB_IMPL_TEX
#define MULIB_IMPL_DSDOM
#define MULIB_IMPL_TAS
#define MULIB_IMPL_XTAS
#define MULIB_IMPL_PANDOCAST
#define MULIB_IMPL_GVIZZ
#define MULIB_IMPL_DYEGRAM
#define MULIB_IMPL_XML
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CSS
#define MULIB_IMPL_JSON
#define MULIB_IMPL_YAML
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_TERMSYN
#define MULIB_IMPL_EDBUF
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#include "mulib-docstring.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char extension[] =
        "ldocstr.element('kbd', function(content, attrs)\n"
        "  attrs.inline = 'true'\n"
        "  return ldocstr.Element('kbd', content, attrs)\n"
        "end)\n";
    const char source[] =
        ".file example.c\n"
        ".define PRODUCT MuLIB\n"
        ".define greet(who) Hello, **${who}**!\n"
        "# {{PRODUCT}} documentation\n"
        ".toc\n\n"
        "{{greet(reader)}} Press @kbd{Enter}. `code` and __underline__.\n\n"
        "> A structured quotation.\n\n"
        "| Element | Purpose |\n| --- | --- |\n| DocstrIL | Shared IR |\n\n"
        ".graph\nparse -> filter: DocstrIL\nfilter -> output: render\n.endgraph\n"
        ".index Renderer\n.indices\n"
        ".style css\nheading { color: navy; } kbd { font-weight: bold; }\n.endstyle\n";
    mulib_docstrings_t *compiler = mulib_docstrings_create(NULL);
    mulib_docstrings_error_t error;
    mulib_docstril_t *document;
    mulib_tas_t *tokens;
    char *html;
    assert(compiler);
    assert(mulib_docstrings_extend(compiler,extension,sizeof extension-1,&error) == MULIB_DOCSTRINGS_OK);
    document = mulib_docstrings_compile(compiler,source,sizeof source-1,NULL,&error);
    if (!document) fprintf(stderr,"%s:%zu:%zu: %s\n",error.source,error.line,error.column,error.message);
    assert(document);
    tokens = mulib_tas_from_dom(document); assert(tokens && mulib_tas_count(tokens));
    html = mulib_docstrings_render(document,"html",NULL,&error); assert(html);
    puts(html);
    free(html); mulib_tas_destroy(tokens); mulib_xml_destroy(document); mulib_docstrings_destroy(compiler);
    return 0;
}
