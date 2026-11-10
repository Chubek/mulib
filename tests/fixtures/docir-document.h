#ifndef MULIB_TEST_DOCIR_DOCUMENT_H
#define MULIB_TEST_DOCIR_DOCUMENT_H
/* Test-only construction. Uses semantic APIs, never private ownership records. */
static inline mulib_docir_node_t *mulib_test_docir_child_(mulib_docir_node_t *parent,unsigned kind,const char *text) {
    mulib_docir_node_t *node=mulib_docir_new(kind,text);
    if(node&&!mulib_docir_append(parent,node)){mulib_docir_destroy(node);return NULL;}
    return node;
}
static inline mulib_docir_node_t *mulib_test_docir_document_(void) {
    mulib_docir_node_t *root=mulib_docir_new(mulib_docir_ROOT,NULL),*section,*p,*node,*math,*fraction,*row,*radical,*list,*figure,*bib;
    if(!root)return NULL;
    if(!mulib_docir_set_id(root,"publication")||
       !(section=mulib_test_docir_child_(root,mulib_docir_SECTION,NULL))||!mulib_docir_set_id(section,"intro")||
       !mulib_test_docir_child_(section,mulib_docir_HEADING,"An & example")||
       !(p=mulib_test_docir_child_(section,mulib_docir_PARAGRAPH,NULL))||
       !mulib_docir_set_source(p,"fixture.tex",0,12,1,1)||
       !mulib_test_docir_child_(p,mulib_docir_TEXT,"Hello ")||
       !mulib_test_docir_child_(p,mulib_docir_TEXT,"<world> & 100% "))goto fail;
    mulib_docir_style_t style={mulib_docir_BOLD,12*65536,0};
    if(!mulib_docir_set_style(p,&style)||!(node=mulib_test_docir_child_(p,mulib_docir_LINK,"visit"))||
       !mulib_docir_set_target(node,"https://example.test/?a=1&b=2")||
       !(node=mulib_test_docir_child_(p,mulib_docir_REFERENCE,"the section"))||!mulib_docir_set_target(node,"#intro")||
       !(node=mulib_test_docir_child_(p,mulib_docir_CITE,NULL))||!mulib_docir_set_target(node,"#referenceone")||
       !mulib_test_docir_child_(p,mulib_docir_FOOTNOTE,"A safe footnote.")||
       !mulib_test_docir_child_(p,mulib_docir_LINEBREAK,NULL)||
       !mulib_test_docir_child_(p,mulib_docir_CODE,"\\input{not-executed}"))goto fail;
    if(!(math=mulib_test_docir_child_(p,mulib_docir_MATH,NULL))||
       !(fraction=mulib_test_docir_child_(math,mulib_docir_MATH_FRACTION,NULL))||
       !(row=mulib_test_docir_child_(fraction,mulib_docir_MATH_ROW,NULL))||
       !mulib_test_docir_child_(row,mulib_docir_MATH_SYMBOL,"a")||
       !mulib_test_docir_child_(row,mulib_docir_MATH_SYMBOL,"+")||
       !mulib_test_docir_child_(row,mulib_docir_MATH_SYMBOL,"1")||
       !(radical=mulib_test_docir_child_(fraction,mulib_docir_MATH_RADICAL,NULL))||
       !mulib_test_docir_child_(radical,mulib_docir_MATH_SYMBOL,"b"))goto fail;
    if(!(list=mulib_test_docir_child_(section,mulib_docir_LIST,NULL))||
       !mulib_test_docir_child_(list,mulib_docir_ITEM,"First <item>")||
       !mulib_test_docir_child_(list,mulib_docir_ITEM,"Second item"))goto fail;
    if(!(figure=mulib_test_docir_child_(root,mulib_docir_FIGURE,NULL))||!mulib_docir_set_id(figure,"figure-one")||
       !mulib_test_docir_child_(figure,mulib_docir_PARAGRAPH,"Figure body text")||
       !mulib_test_docir_child_(figure,mulib_docir_CAPTION,"Figure & caption"))goto fail;
    if(!(bib=mulib_test_docir_child_(root,mulib_docir_BIBLIOGRAPHY,"References"))||
       !(node=mulib_test_docir_child_(bib,mulib_docir_BIBITEM,"A. Author. A safe title."))||
       !mulib_docir_set_id(node,"referenceone"))goto fail;
    return root;
fail:mulib_docir_destroy(root);return NULL;
}
#endif
