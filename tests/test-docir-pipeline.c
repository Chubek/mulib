#define MULIB_ENABLE_DOCIR
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_DOCIR
#define MULIB_IMPL_DOCVALIDATE
#define MULIB_IMPL_DOCNORMALIZE
#define MULIB_IMPL_DOCSERIALIZE
#define MULIB_IMPL_DOCSOURCE
#define MULIB_IMPL_TYPESRC
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TEX
#define MULIB_IMPL_LATEX
#define MULIB_IMPL_XML
#define MULIB_IMPL_HTML
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_DEFLATE
#define MULIB_IMPL_ZIP
#define MULIB_IMPL_EPUB
#include "mulib-docvalidate.h"
#include "mulib-docnormalize.h"
#include "mulib-docserialize.h"
#include "mulib-docsource.h"
#include "mulib-latex.h"
#include "mulib-epub.h"
#include "fixtures/docir-document.h"
#include <assert.h>

int main(void) {
    mulib_docir_node_t *root=mulib_test_docir_document_();assert(root);
    mulib_docir_report_t report={99,99};assert(mulib_docvalidate_tree(root,NULL,&report)&&report.code==mulib_docir_OK);
    assert(mulib_docir_owner(root->children[0]->children[1])==root);
    assert(mulib_docir_parent(root->children[0])==root);
    assert(!mulib_docir_append(root,root->children[0]));
    mulib_docir_limits_t low={2,0,0};mulib_docir_snapshot_t s={0};
    assert(!mulib_docir_snapshot_make(root,&low,&s,&report)&&report.code==mulib_docir_E_LIMIT&&!s.records);
    assert(mulib_docir_snapshot_make(root,NULL,&s,&report));size_t original_count=s.count;
    unsigned saved=s.records[0].kind;s.records[0].kind=999;
    assert(!mulib_docir_snapshot_validate(&s,0,&report));s.records[0].kind=saved;
    mulib_docir_snapshot_destroy(&s);
    mulib_docir_node_t *normal=mulib_docnormalize_tree(root,NULL,&report);assert(normal);
    assert(mulib_docir_snapshot_make(normal,NULL,&s,NULL)&&s.count==original_count-1);
    mulib_docir_snapshot_destroy(&s);mulib_docir_destroy(normal);
    mulib_typesrc_source_t source={0};assert(mulib_typesrc_init(&source,"first\nsecond",12));
    assert(mulib_docsource_bind(root->children[0],&source,"source.tex",6,12));
    assert(!mulib_docsource_bind(root,&source,"source.tex",12,13));mulib_typesrc_destroy(&source);

    mulib_typecore_buf_t binary={0},again={0},fragment={0};mulib_docir_node_t *decoded=NULL;
    assert(mulib_docserialize_tree(root,NULL,&binary,&report));
    assert(binary.size>12&&!memcmp(binary.data,"MuDocIR\1",8));
    assert(mulib_docserialize_read(binary.data,binary.size,NULL,&decoded,&report));
    assert(mulib_docserialize_tree(decoded,NULL,&again,&report)&&binary.size==again.size&&!memcmp(binary.data,again.data,binary.size));
    mulib_docir_destroy(decoded);decoded=NULL;mulib_typecore_free(&again);
    for(size_t i=0;i<binary.size;i++)assert(!mulib_docserialize_read(binary.data,i,NULL,&decoded,&report)&&!decoded);
    unsigned char saved_byte=binary.data[16];binary.data[16]=0;
    assert(!mulib_docserialize_read(binary.data,binary.size,NULL,&decoded,&report));binary.data[16]=saved_byte;
    assert(mulib_docir_html(root,&fragment)&&mulib_typecore_ends(&fragment));
    assert(strstr((const char*)fragment.data,"<mfrac>")&&strstr((const char*)fragment.data,"<msqrt>"));
    assert(strstr((const char*)fragment.data,"&lt;world&gt;")&&strstr((const char*)fragment.data,"font-size:12.000000pt"));
    mulib_typecore_free(&fragment);
    mulib_latex_t latex={0};assert(mulib_latex_from_docir(&latex,"Safe & title",root));
    assert(latex.state==2&&strstr(latex.source.data,"\\frac{{a+1}}{\\sqrt{b}}"));
    assert(!strstr(latex.source.data,"\\input{not-executed}")&&strstr(latex.source.data,"\\cite{referenceone}"));
    assert(!mulib_latex_from_docir(&latex,"Title",root));mulib_latex_free(&latex);
    mulib_strbuf_t html={0};assert(mulib_html_docir(root,"Safe & title","en",&html));
    assert(strstr(html.data,"Safe &amp; title")&&strstr(html.data,"xmlns=\"http://www.w3.org/1998/Math/MathML\""));
    mulib_xml_error_t error={0};mulib_xml_node_t *xml=mulib_xml_parse(html.data+16,html.length-16,&error);assert(xml);
    mulib_xml_destroy(xml);mulib_strbuf_free(&html);

    mulib_epub_docir_chapter_t chapters[]={{"one.xhtml","First",root},{"two.xhtml","Second",root}};
    mulib_epub_book_t book={"urn:example:docir-test","DocIR","en","MuLIB","2026-11-10T00:00:00Z",NULL,0};
    mulib_buffer_t epub={0},chapter={0};assert(!mulib_epub_write_docir(&book,chapters,2,&epub));
    mulib_epub_reader_t reader={0};assert(!mulib_epub_open(&reader,epub.data,epub.length,1024u*1024u)&&reader.spine_count==2);
    assert(!mulib_epub_read(&reader,reader.spine[0],1024u*1024u,&chapter));
    assert(reader.items[reader.spine[0]].properties&&strstr(reader.items[reader.spine[0]].properties,"mathml"));
    xml=mulib_xml_parse((const char*)chapter.data,chapter.length,&error);assert(xml);mulib_xml_destroy(xml);
    mulib_epub_close(&reader);mulib_buffer_free(&chapter);
    size_t before=epub.length;chapters[1].path="one.xhtml";
    assert(mulib_epub_write_docir(&book,chapters,2,&epub)<0&&epub.length==before);mulib_buffer_free(&epub);
    mulib_typecore_free(&binary);mulib_docir_destroy(root);

    /* The public node layout is unchanged; unregistered/aliased/cyclic graphs fail. */
    root=mulib_docir_new(mulib_docir_ROOT,NULL);mulib_docir_node_t *a=mulib_docir_new(mulib_docir_SECTION,NULL);
    mulib_docir_node_t *b=mulib_docir_new(mulib_docir_SECTION,NULL);assert(root&&a&&b);
    assert(mulib_docir_append(root,a)&&mulib_docir_append(a,b));
    assert(!mulib_docir_append(b,a));assert(mulib_docir_detach(root,0)==a);
    assert(!mulib_docir_append(b,a));assert(mulib_docir_append(root,a));
    mulib_docir_node_t forged={0};assert(!mulib_docir_append(root,&forged));
    assert(mulib_typecore_text(&fragment,"prefix"));root->children[0]=root;
    assert(!mulib_docir_html(root,&fragment)&&fragment.size==6&&!memcmp(fragment.data,"prefix",6));
    mulib_docir_destroy(root);mulib_typecore_free(&fragment);
    root=mulib_docir_new(mulib_docir_ROOT,NULL);a=mulib_docir_new(mulib_docir_PARAGRAPH,NULL);
    assert(root&&a&&mulib_docir_append(root,a));mulib_docir_destroy(a);assert(root->count==0);mulib_docir_destroy(root);
    assert(!mulib_docir_new(mulib_docir_TEXT,"\xc0\x80"));
    root=mulib_docir_new(mulib_docir_ROOT,NULL);a=mulib_docir_new(mulib_docir_LINK,"unsafe");
    assert(root&&a&&mulib_docir_append(root,a));assert(!mulib_docir_set_target(a,"JaVaScRiPt:alert(1)"));
    assert(mulib_docir_set_target(a,"#absent")&&!mulib_docvalidate_tree(root,NULL,&report)&&report.code==mulib_docir_E_REFERENCE);
    mulib_docir_destroy(root);
    /* Build bottom-up so adversarial depth cannot overflow recursive teardown. */
    root=mulib_docir_new(mulib_docir_ROOT,NULL);a=mulib_docir_new(mulib_docir_SECTION,NULL);assert(root&&a);
    for(size_t i=0;i<5000;i++) {
        b=mulib_docir_new(mulib_docir_SECTION,NULL);assert(b&&mulib_docir_append(b,a));a=b;
    }
    assert(mulib_docir_append(root,a));
    assert(!mulib_docir_snapshot_make(root,NULL,&s,&report)&&report.code==mulib_docir_E_LIMIT&&!s.records);
    mulib_docir_destroy(root);return 0;
}
