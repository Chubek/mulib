#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PRETTYPRINT_TEST_SEPARATE
#include "mulib-pretty-standalone.inc"
#else
#include "mulib-prettysexpr.h"
#endif

_Static_assert(_Generic(&mulib_pretty_parse_expr,
 int (*)(const char *,mulib_prettydoc_t **,mulib_pretty_id_t *):1,default:0),
 "PrettySexpr parser ABI");
_Static_assert(_Generic(&mulib_pretty_render_expr,
 char *(*)(const char *,const mulib_pretty_options_t *,mulib_pretty_target_t):1,default:0),
 "PrettySexpr renderer ABI");

static char *formatted(const char *spec,size_t width,int mode,int target){
 mulib_pretty_options_t opt;mulib_pretty_options_init(&opt);opt.width=width;opt.algorithm=(mulib_pretty_algorithm_t)mode;
 return mulib_pretty_render_expr(spec,&opt,(mulib_pretty_target_t)target);
}
static void expect_plain(const char *spec,const mulib_pretty_options_t *opt,const char *expected){
 char *s=mulib_pretty_render_expr(spec,opt,MULIB_PRETTY_PLAIN);
 if(!s || strcmp(s,expected))fprintf(stderr,"%s\nexpected: [%s]\nactual:   [%s]\n",spec,expected,s?s:"(null)");
 assert(s && !strcmp(s,expected));
 free(s);
}
static void test_verbatim(void){
 const char *spec="(nest 4 (cat (hard) (source 9 1 12 (annot \"string\" (verbatim \"a\\r\\n\\tb\\n.c\")))))";
 mulib_pretty_options_t o;mulib_pretty_options_init(&o);
 expect_plain(spec,&o,"\n    a\r\n\tb\n.c");
 expect_plain("(nest 4 (text \"a\\nb\"))",&o,"a\n    b");
 expect_plain("(group (cat (text \"a\") (cat (soft) (verbatim \"b\\nc\"))))",&o,"a\nb\nc");
 assert(!mulib_pretty_render_expr("(verbatim \"\\x00\")",&o,MULIB_PRETTY_PLAIN));
 mulib_prettydoc_t *d=NULL;mulib_pretty_id_t root;
 assert(mulib_pretty_parse_expr(spec,&d,&root));
 mulib_pretty_layout_t l={0};
 assert(mulib_pretty_layout(d,root,&o,&l));
 assert(l.lines==4 && l.max_columns==6 && l.map_count==1);
 assert(l.map[0].start==5 && l.map[0].length==8 && l.map[0].origin.source==9);
 assert(l.count==2 && l.pieces[1].style==MULIB_PRETTY_STRING && l.pieces[1].length==8);
 char *roff=mulib_pretty_emit(&l,MULIB_PRETTY_TROFF);
 assert(roff&&strstr(roff,"a<CR>\n\tb\n\\&.c"));
 free(roff);
 mulib_pretty_layout_free(&l);mulib_prettydoc_free(d);
}
static void test_source_and_incremental(void){
 mulib_prettydoc_t *d=NULL;mulib_pretty_id_t a;
 assert(mulib_pretty_parse_expr("(source 1 10 24 (cat (text \"é\") (cat (hard) (source 2 0 1 (text \"x\")))))",&d,&a));
 mulib_pretty_id_t b=mulib_pretty_source(d,(mulib_pretty_origin_t){3,1,2},mulib_pretty_text(d,"y"));
 mulib_pretty_id_t c=mulib_pretty_source(d,(mulib_pretty_origin_t){4,2,3},mulib_pretty_text(d,"z"));
 mulib_pretty_session_t *s=mulib_pretty_session_new(d);assert(s&&!mulib_pretty_session_view(s));
 mulib_pretty_options_t o;mulib_pretty_options_init(&o);
 size_t hits=99;mulib_pretty_id_t roots[]={a,b};
 assert(mulib_pretty_session_update(s,roots,2,&o,&hits)&&hits==0);
 const mulib_pretty_layout_t *view=mulib_pretty_session_view(s);
 assert(!strcmp(view->text,"é\nx\ny")&&view->map_count==3&&view->lines==3);
 assert(view->map[0].start==0&&view->map[0].length==2&&view->map[0].origin.start==10);
 assert(view->map[1].start==3&&view->map[1].origin.source==2);
 assert(view->map[2].start==5&&view->map[2].origin.source==3);
 roots[0]=c;roots[1]=a;
 assert(mulib_pretty_session_update(s,roots,2,&o,&hits)&&hits==1);
 view=mulib_pretty_session_view(s);
 assert(!strcmp(view->text,"z\né\nx")&&view->map[1].start==2&&view->map[2].start==5);
 mulib_pretty_id_t fresh=mulib_pretty_cat(d,c,mulib_pretty_cat(d,mulib_pretty_hardline(d),a));
 mulib_pretty_layout_t oracle={0};assert(mulib_pretty_layout(d,fresh,&o,&oracle));
 assert(!strcmp(view->text,oracle.text)&&view->map_count==oracle.map_count);
 for(size_t i=0;i<view->map_count;i++){
  assert(view->map[i].start==oracle.map[i].start&&view->map[i].length==oracle.map[i].length&&
         view->map[i].origin.source==oracle.map[i].origin.source);
 }
 mulib_pretty_layout_free(&oracle);
 for(int t=0;t<=3;t++){
  char *first=mulib_pretty_emit(view,(mulib_pretty_target_t)t);
  assert(mulib_pretty_session_update(s,roots,2,&o,&hits)&&hits==2);
  view=mulib_pretty_session_view(s);
  char *next=mulib_pretty_emit(view,(mulib_pretty_target_t)t);
  assert(first&&next&&!strcmp(first,next));free(first);free(next);
 }
 roots[0]=MULIB_PRETTY_NONE;
 assert(!mulib_pretty_session_update(s,roots,2,&o,&hits)&&hits==2&&!strcmp(view->text,"z\né\nx"));
 roots[0]=c;o.width=40;
 assert(mulib_pretty_session_update(s,roots,2,&o,&hits)&&hits==0);
 assert(mulib_pretty_session_update(s,NULL,0,&o,&hits));
 view=mulib_pretty_session_view(s);assert(view&&!view->length&&view->lines==1);
 mulib_pretty_session_free(s);mulib_prettydoc_free(d);
}

static void test_math(void){
 const char *spec="(matrix (row (frac (math-atom \"<&\") (root (math-atom \"2\")))"
  " (sup (math-atom \"x\") (math-atom \"n\")))"
  " (row (sub (math-atom \"a\") (math-atom \"i\")) (math-atom \"\\\"\\\\\")))";
 mulib_pretty_options_t o;mulib_pretty_options_init(&o);
 expect_plain(spec,&o,"[[(<&)/(sqrt(2)), (x)^(n)]; [(a)_(i), \"\\]]");
 char *s=mulib_pretty_render_expr(spec,&o,MULIB_PRETTY_HTML_MATHML);
 assert(s&&strstr(s,"<mtable><mtr><mtd><mfrac>")&&strstr(s,"<msqrt>")&&strstr(s,"<msup>")&&
        strstr(s,"<msub>")&&strstr(s,"&lt;&amp;"));free(s);
 s=mulib_pretty_render_expr(spec,&o,MULIB_PRETTY_LATEX);
 assert(s&&strstr(s,"\\begin{matrix}\\frac{\\text{<\\&}}{\\sqrt{")&&strstr(s,"\\end{matrix}"));free(s);
 s=mulib_pretty_render_expr(spec,&o,MULIB_PRETTY_TROFF);
 assert(s&&strstr(s,".EQ\n matrix {  lcol")&&strstr(s,"above")&&strstr(s,"\\N'034'\\N'092'"));free(s);
 assert(!mulib_pretty_render_expr("(matrix (row (math-atom \"a\")) (row))",&o,MULIB_PRETTY_PLAIN));
 assert(!mulib_pretty_render_expr("(root (text \"a\"))",&o,MULIB_PRETTY_PLAIN));
 assert(!mulib_pretty_render_expr("(root (math-atom \"\\n.EN\"))",&o,MULIB_PRETTY_TROFF));
}

static void test_knuth_plass(void){
 mulib_pretty_options_t o;mulib_pretty_options_init(&o);
 o.paragraph=MULIB_PRETTY_KNUTH_PLASS;o.width=9;
 expect_plain("(cat (text \"// \") (fill \"aa bb cccc dddd\"))",&o,"// aa bb\ncccc dddd");
 o.width=10;o.ribbon_width=7;
 expect_plain("(nest 2 (cat (hard) (fill \"abc abc abc abc\")))",&o,"\n  abc abc\n  abc abc");
 o.width=6;o.ribbon_width=0;
 expect_plain("(source 1 0 11 (fill \"aa ab\xc2\xad""cdef\"))",&o,"aa ab-\ncdef");
 assert(!mulib_pretty_render_expr("(fill \"unbreakable\")",&o,MULIB_PRETTY_PLAIN));
}

static void test_unicode_width(void){
 /* U+754C is wide, U+00E9 is narrow, and U+0301 is nonspacing. */
 const char *wide="\xe7\x95\x8c",*accent="\xc3\xa9",*combining="e\xcc\x81";
 const char *group="(group (cat (text \"\xe7\x95\x8c\") (cat (soft) (text \"e\xcc\x81\"))))";
 mulib_pretty_options_t opt;mulib_pretty_options_init(&opt);
 for(int mode=MULIB_PRETTY_GREEDY;mode<=MULIB_PRETTY_SEARCH;mode++){
  opt.algorithm=(mulib_pretty_algorithm_t)mode;opt.width=4;
  expect_plain(group,&opt,"\xe7\x95\x8c e\xcc\x81");
  opt.width=3;
  expect_plain(group,&opt,"\xe7\x95\x8c\ne\xcc\x81");
 }
 opt.algorithm=MULIB_PRETTY_FITS;opt.width=10;opt.ribbon_width=4;
 for(int mode=MULIB_PRETTY_WORD_GREEDY;mode<=MULIB_PRETTY_MIN_RAGGEDNESS;mode++){
  opt.paragraph=(mulib_pretty_paragraph_t)mode;
  expect_plain("(fill \"\xc3\xa9 \t \xe7\x95\x8c e\xcc\x81\")",&opt,"\xc3\xa9 \xe7\x95\x8c\ne\xcc\x81");
 }
 opt.ribbon_width=0;
 expect_plain("(cat (text \"\xe7\x95\x8c\") (align (cat (hard) (text \"x\"))))",&opt,
              "\xe7\x95\x8c\n  x");
 mulib_prettydoc_t *doc=mulib_prettydoc_new();assert(doc);
 mulib_pretty_id_t root=mulib_pretty_cat(doc,mulib_pretty_text(doc,wide),
   mulib_pretty_annotate(doc,MULIB_PRETTY_IDENTIFIER,mulib_pretty_text(doc,combining)));
 mulib_pretty_layout_t layout={0};
 assert(mulib_pretty_layout(doc,root,&opt,&layout));
 assert(layout.max_columns==3 && layout.length==6 && layout.count==2);
 assert(layout.pieces[1].start==3 && layout.pieces[1].length==3);
 mulib_pretty_layout_free(&layout);
 root=mulib_pretty_text(doc,accent);
 assert(mulib_pretty_layout(doc,root,&opt,&layout) && layout.max_columns==1);
 mulib_pretty_layout_free(&layout);
 opt.indent_limit=0;opt.max_output=1;
 assert(!mulib_pretty_layout(doc,root,&opt,&layout) && !layout.text && !layout.pieces);
 /* Width probes must also charge when scanning repeated zero-width text. */
 char marks[2049];
 for(size_t i=0;i<sizeof(marks)-1;i+=2){marks[i]=(char)0xcc;marks[i+1]=(char)0x81;}
 marks[sizeof(marks)-1]=0;
 root=mulib_pretty_text(doc,marks);
 for(unsigned i=0;i<13;i++)root=mulib_pretty_cat(doc,root,root);
 root=mulib_pretty_group(doc,root);
 assert(root!=MULIB_PRETTY_NONE);
 opt.width=1;opt.max_output=1;
 assert(!mulib_pretty_layout(doc,root,&opt,&layout) && !layout.text && !layout.pieces);
 mulib_prettydoc_free(doc);
}

static void test_ribbon(void){
 const char *group="(group (cat (text \"abc\") (cat (soft) (text \"def\"))))";
 mulib_pretty_options_t opt;mulib_pretty_options_init(&opt);opt.width=10;
 expect_plain(group,&opt,"abc def");
 opt.ribbon_width=6;
 for(int mode=MULIB_PRETTY_GREEDY;mode<=MULIB_PRETTY_SEARCH;mode++){
  opt.algorithm=(mulib_pretty_algorithm_t)mode;
  expect_plain(group,&opt,"abc\ndef");
 }
 opt.algorithm=MULIB_PRETTY_FITS;
 /* Entering a nest in the middle of a line doesn't extend its ribbon. */
 expect_plain("(cat (text \"px\") (nest 2 (group (cat (text \"ab\") (cat (soft) (text \"cd\"))))))",
              &opt,"pxab\n  cd");
 /* A break inside a nest measures the next line after the emitted indent.
  * That indent still counts against the page width. */
 const char *nested="(nest 2 (cat (hard) (group (cat (text \"abc\") (cat (soft) (text \"de\"))))))";
 expect_plain(nested,&opt,"\n  abc de");
 opt.width=7;
 expect_plain(nested,&opt,"\n  abc\n  de");
 opt.width=10;
 expect_plain("(choice (text \"1234567\") (text \"short\"))",&opt,"short");
 for(int mode=MULIB_PRETTY_WORD_GREEDY;mode<=MULIB_PRETTY_MIN_RAGGEDNESS;mode++){
  opt.paragraph=(mulib_pretty_paragraph_t)mode;
  expect_plain("(nest 2 (cat (hard) (fill \"abc de fg\")))",&opt,"\n  abc de\n  fg");
  expect_plain("(fill \"abcdefg h\")",&opt,"abcdefg\nh");
 }
 /* A ribbon wider than the page is harmless, even at the size_t limit. */
 opt.ribbon_width=SIZE_MAX;
 expect_plain(group,&opt,"abc def");
}
int main(void){
 test_verbatim();
 test_ribbon();
 test_unicode_width();
 test_source_and_incremental();
 test_math();
 test_knuth_plass();
 const char *spec="(group (cat (text \"call(\") (cat (nest 2 (cat (soft) (text \"alpha,beta\"))) (cat (soft) (text \")\")))))";
 char *s=formatted(spec,80,MULIB_PRETTY_FITS,MULIB_PRETTY_PLAIN);
 assert(s);assert(strcmp(s,"call( alpha,beta )")==0);free(s);
 s=formatted(spec,8,MULIB_PRETTY_FITS,MULIB_PRETTY_PLAIN);
 assert(s);assert(strcmp(s,"call(\n  alpha,beta\n)")==0);free(s);
 s=formatted(spec,8,MULIB_PRETTY_SEARCH,MULIB_PRETTY_PLAIN);
 assert(s);assert(strstr(s,"alpha,beta"));free(s);
 s=formatted("(group (cat (text \"a\\nb\") (cat (soft) (text \"c\"))))",80,1,MULIB_PRETTY_PLAIN);
 assert(s);assert(strcmp(s,"a\nb\nc")==0);free(s);
 s=formatted("(annot \"keyword\" (text \"<&\\\"\"))",80,1,MULIB_PRETTY_HTML_MATHML);
 assert(s);assert(strstr(s,"tok-keyword"));assert(strstr(s,"&lt;&amp;&quot;"));free(s);
 s=formatted("(annot \"math-ident\" (text \"x\"))",80,1,MULIB_PRETTY_HTML_MATHML);
 assert(s);assert(strstr(s,"<math xmlns="));assert(strstr(s,"<mi>x</mi>"));free(s);
 s=formatted("(text \".foo\\\\bar\")",80,1,MULIB_PRETTY_TROFF);
 assert(s);assert(strstr(s,"\\&.foo\\ebar"));free(s);
 s=formatted("(text \"a_b%$\\\\\")",80,1,MULIB_PRETTY_LATEX);
 assert(s);assert(strstr(s,"\\_"));assert(strstr(s,"\\%"));assert(strstr(s,"\\textbackslash{}"));free(s);
 s=formatted("(fill \"alpha beta gamma delta\")",12,1,MULIB_PRETTY_PLAIN);
 assert(s);assert(strchr(s,'\n'));free(s);
 s=formatted("(annot \"comment\" (fill \"alpha beta\"))",12,1,MULIB_PRETTY_HTML_MATHML);
 assert(s);assert(strstr(s,"<span class=\"tok-comment\">alpha beta</span>"));free(s);
 mulib_pretty_options_t opt;mulib_pretty_options_init(&opt);opt.indent_limit=4;opt.max_output=4;
 assert(!mulib_pretty_render_expr("(fill \"alpha beta\")",&opt,MULIB_PRETTY_PLAIN));
 opt.max_output=128;
 assert(!mulib_pretty_render_expr("(cat (text \"abcde\") (align (nest 1 (text \"x\"))))",&opt,MULIB_PRETTY_PLAIN));
 /* In this search, the broken candidate fits the byte budget and the flat
  * candidate exceeds it; failure must also release the saved candidate. */
 opt.width=4;opt.max_output=5;opt.algorithm=MULIB_PRETTY_SEARCH;
 assert(!mulib_pretty_render_expr("(cat (group (cat (text \"abc\") (soft)))"
                                " (choice (text \"x\") (text \"yyyyy\")))",&opt,MULIB_PRETTY_PLAIN));
 s=formatted("(cat (region \"greedy\" (cat (text \"abcd\") (cat (soft) (text \"e\")))) (cat (hard) (region \"fits\" (group (cat (text \"a\") (cat (soft) (text \"b\")))))))",6,1,MULIB_PRETTY_PLAIN);
 assert(s);assert(strcmp(s,"abcd e\na b")==0);free(s);
 s=formatted("(paragraph \"ragged\" (fill \"a bb ccc dd eee\"))",7,1,MULIB_PRETTY_PLAIN);
 assert(s);assert(strchr(s,'\n'));free(s);
 s=formatted("(region \"search\" (group (cat (text \"hello\") (cat (soft) (text \"world\")))))",8,1,MULIB_PRETTY_PLAIN);
 assert(s);assert(strchr(s,'\n'));free(s);
 mulib_prettydoc_t *bad_doc=mulib_prettydoc_new();
 assert(bad_doc);const char bad_utf8[]={ (char)0xc0, (char)0xaf, 0 };
 assert(mulib_pretty_text(bad_doc,bad_utf8)==MULIB_PRETTY_NONE);
 mulib_prettydoc_free(bad_doc);
 assert(!formatted("(text \"bad\") trailing",80,1,MULIB_PRETTY_PLAIN));
 assert(!formatted("(unknown \"bad\")",80,1,MULIB_PRETTY_PLAIN));
 assert(!formatted("(nest 999999 (text \"hi\"))",80,1,MULIB_PRETTY_PLAIN));
 s=formatted("(text \"caf\\xc3\\xA9\\x0a\")",80,1,MULIB_PRETTY_PLAIN);
 assert(s);assert(strcmp(s,"caf\xc3\xa9\n")==0);free(s);
 assert(!formatted("(text \"\\x00\")",80,1,MULIB_PRETTY_PLAIN));
 assert(!formatted("(text \"\\xc3\")",80,1,MULIB_PRETTY_PLAIN));
 assert(!formatted("(text \"\\xef\\xbf\\xbe\")",80,1,MULIB_PRETTY_PLAIN));
 assert(!formatted("(text \"\\x",80,1,MULIB_PRETTY_PLAIN));
 assert(!formatted("(text \"\\xa",80,1,MULIB_PRETTY_PLAIN));
 assert(!formatted("(text \"\\xag\")",80,1,MULIB_PRETTY_PLAIN));
 mulib_prettydoc_t *sentinel_doc=NULL;
 mulib_pretty_id_t sentinel_root=42;
 assert(!mulib_pretty_parse_expr("(text \"first\") (soft)",&sentinel_doc,&sentinel_root));
 assert(!sentinel_doc&&sentinel_root==42);
 assert(mulib_pretty_parse_expr("(text \"\")",&sentinel_doc,&sentinel_root));
 assert(sentinel_doc && sentinel_root!=MULIB_PRETTY_NONE);
 mulib_prettydoc_free(sentinel_doc);
 assert(!mulib_pretty_render_expr("(text \"\")",NULL,MULIB_PRETTY_PLAIN));
 s=formatted("(text \"\")",80,MULIB_PRETTY_FITS,MULIB_PRETTY_PLAIN);
 assert(s && !*s);free(s);
 /* A shallow DAG can expand to billions of nodes even with no output. */
 mulib_prettydoc_t *dag=mulib_prettydoc_new();assert(dag);
 mulib_pretty_id_t root=mulib_pretty_text(dag,"");
 for(unsigned i=0;i<36;i++){root=mulib_pretty_cat(dag,root,root);assert(root!=MULIB_PRETTY_NONE);}
 mulib_pretty_layout_t layout={0};mulib_pretty_options_init(&opt);
 assert(!mulib_pretty_layout(dag,root,&opt,&layout)&&!layout.text&&!layout.pieces);
 mulib_prettydoc_free(dag);
 puts("prettyprint C11 tests passed");return 0;
}
