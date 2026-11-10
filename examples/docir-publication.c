/* One semantic tree -> normalized DocIR v1, HTML/MathML, LaTeX and EPUB.
 * Build: cc -std=c11 -Wall -Wextra -Iinclude examples/docir-publication.c -lm -o docir-publication
 * Run without arguments for in-memory checks, or pass an existing output directory.
 */
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

static mulib_docir_node_t *add_(mulib_docir_node_t *parent,unsigned kind,const char *text) {
    mulib_docir_node_t *node=mulib_docir_new(kind,text);
    if(node&&!mulib_docir_append(parent,node)){mulib_docir_destroy(node);return NULL;}
    return node;
}
static mulib_docir_node_t *document_(void) {
    mulib_docir_node_t *root=mulib_docir_new(mulib_docir_ROOT,NULL),*heading,*p,*math,*fraction,*node,*list,*figure,*bibliography;
    if(!root)return NULL;
    if(!(heading=add_(root,mulib_docir_HEADING,"Semantic publication"))||!mulib_docir_set_id(heading,"overview")||
       !(p=add_(root,mulib_docir_PARAGRAPH,NULL))||
       !add_(p,mulib_docir_TEXT,"One semantic tree ")||!add_(p,mulib_docir_TEXT,"targets several publication formats. ")||
       !(node=add_(p,mulib_docir_REFERENCE,"Return to the heading."))||!mulib_docir_set_target(node,"#overview")||
       !(node=add_(p,mulib_docir_LINK,"External link"))||!mulib_docir_set_target(node,"https://example.test/a_b?x=100%25&y=2#part")||
       !add_(p,mulib_docir_FOOTNOTE,"A semantic footnote, not a positioned page insertion.")||
       !(math=add_(p,mulib_docir_MATH,NULL))||!(fraction=add_(math,mulib_docir_MATH_FRACTION,NULL))||
       !add_(fraction,mulib_docir_MATH_SYMBOL,"a")||!add_(fraction,mulib_docir_MATH_SYMBOL,"b"))goto fail;
    mulib_docir_style_t style={mulib_docir_BOLD,12*65536,0};
    if(!mulib_docir_set_style(p,&style)||!mulib_docir_set_source(p,"example.mudoc",0,12,1,1)||
       !(list=add_(root,mulib_docir_LIST,NULL))||!add_(list,mulib_docir_ITEM,"Escaped text: < & >")||
       !add_(list,mulib_docir_ITEM,"Stable kinds, source spans and print-unit styles")||
       !(figure=add_(root,mulib_docir_FIGURE,NULL))||
       !add_(figure,mulib_docir_PARAGRAPH,"A text-only semantic figure.")||
       !add_(figure,mulib_docir_CAPTION,"A safe caption")||
       !(p=add_(root,mulib_docir_PARAGRAPH,NULL))||
       !add_(p,mulib_docir_CODE,"\\input{not-executed}")||
       !(node=add_(p,mulib_docir_CITE,NULL))||!mulib_docir_set_target(node,"#manual")||
       !(bibliography=add_(root,mulib_docir_BIBLIOGRAPHY,"References"))||
       !(node=add_(bibliography,mulib_docir_BIBITEM,"MuLIB. Publishing additions: documented subsets."))||
       !mulib_docir_set_id(node,"manual"))goto fail;
    return root;
fail:mulib_docir_destroy(root);return NULL;
}
static int write_(const char *directory,const char *name,const void *bytes,size_t size) {
    char path[2048];int n=snprintf(path,sizeof(path),"%s/%s",directory,name);
    if(n<0||(size_t)n>=sizeof(path))return 0;
    FILE *file=fopen(path,"wb");if(!file)return 0;
    size_t written=fwrite(bytes,1,size,file);int closed=fclose(file);
    if(written!=size||closed){remove(path);return 0;}return 1;
}
int main(int argc,char **argv) {
    if(argc>2)return 2;
    mulib_docir_node_t *input=document_(),*root=NULL,*second=NULL,*restored=NULL;
    mulib_typecore_buf_t binary={0};mulib_strbuf_t html={0};mulib_latex_t latex={0};mulib_buffer_t epub={0};int rc=1;
    if(!input||!mulib_docvalidate_tree(input,NULL,NULL)||!(root=mulib_docnormalize_tree(input,NULL,NULL))||
       !(second=mulib_docir_clone(root,NULL,0,NULL))||!mulib_docir_set_text(second->children[0],"Second chapter")||
       !mulib_docserialize_tree(root,NULL,&binary,NULL)||
       !mulib_docserialize_read(binary.data,binary.size,NULL,&restored,NULL)||
       !mulib_docvalidate_tree(restored,NULL,NULL)||
       !mulib_html_docir(root,"Semantic publication","en",&html)||
       !mulib_latex_from_docir(&latex,"Semantic publication",root))goto done;
    mulib_epub_book_t metadata={"urn:example:mulib:semantic-publication","Semantic publication","en","MuLIB",
                               "2026-11-10T00:00:00Z",NULL,0};
    mulib_epub_docir_chapter_t chapters[]={{"first.xhtml","First chapter",root},{"second.xhtml","Second chapter",second}};
    if(mulib_epub_write_docir(&metadata,chapters,2,&epub))goto done;
    if(argc==2&&(!write_(argv[1],"document.docir",binary.data,binary.size)||
       !write_(argv[1],"document.html",html.data,html.length)||
       !write_(argv[1],"document.tex",latex.source.data,latex.source.length)||
       !write_(argv[1],"document.epub",epub.data,epub.length)))goto done;
    puts("Semantic DocIR -> HTML/MathML, LaTeX and two-chapter EPUB passed.");rc=0;
done:
    mulib_docir_destroy(restored);mulib_docir_destroy(second);mulib_docir_destroy(root);mulib_docir_destroy(input);
    mulib_typecore_free(&binary);mulib_strbuf_free(&html);mulib_latex_free(&latex);mulib_buffer_free(&epub);return rc;
}
