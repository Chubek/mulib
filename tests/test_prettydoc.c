#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PRETTYPRINT_TEST_SEPARATE
#define MULIB_IMPL_PRETTYDOC
#define MULIB_IMPL_PRETTYEMIT
#define MULIB_IMPL_PRETTYSEXPR
#endif
#include "mulib-prettysexpr.h"

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
 test_ribbon();
 test_unicode_width();
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
