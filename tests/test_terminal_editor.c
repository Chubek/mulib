#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_EDBUF
#define MULIB_IMPL_TERMSYN
#include "mulib-termsyn.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void styling(void) {
  const char *css = "code {color:#abc; background-color:rgb(1, 2, 3)} .keyword {color:red!important; font-weight:bold} #x {color:blue}";
  mulib_css_sheet_t *sheet = mulib_css_parse(css, strlen(css), NULL);
  mulib_xml_node_t *node = mulib_xml_new(MULIB_XML_ELEMENT, "code");
  mulib_css_style_t *d;
  mulib_term_style_t style, saved, parent = mulib_term_style_default();
  mulib_terminfo_t info;
  mulib_strbuf_t output;
  mulib_xml_error_t error;
  const char injection[] = "hello\033[31m\x7f\xc2\x9b\n\t";
  assert(sheet && node && mulib_xml_set_attr(node,"class","keyword") && mulib_xml_set_attr(node,"id","x"));
  assert(mulib_xml_set_attr(node,"style","color: lime; font-style:italic; text-decoration:underline line-through"));
  assert(!mulib_termstyle_resolve(sheet,node,NULL,&style,&error));
  assert(style.foreground == MULIB_TERM_RGB(255,0,0) && style.background == MULIB_TERM_RGB(1,2,3));
  assert(style.attributes == (MULIB_TERM_BOLD|MULIB_TERM_ITALIC|MULIB_TERM_UNDERLINE|MULIB_TERM_STRIKE));
  mulib_terminfo_init(&info); mulib_terminfo_ansi(&info);
  mulib_strbuf_init(&output);
  assert(!mulib_termstyle_write(style,&info,MULIB_TERMSTYLE_TRUECOLOR,injection,sizeof injection-1,&output));
  assert(strstr(output.data,"\033[38;2;255;0;0m") && strstr(output.data,"hello\xef\xbf\xbd[31m"));
  assert(!strstr(output.data,"hello\033"));
  mulib_strbuf_free(&output);
  mulib_strbuf_init(&output);
  assert(!mulib_termstyle_write(style,NULL,MULIB_TERMSTYLE_PLAIN,injection,sizeof injection-1,&output));
  assert(!strchr(output.data,'\033'));
  assert(strstr(output.data,"\xef\xbf\xbd[31m\xef\xbf\xbd\xef\xbf\xbd\n\t"));
  saved = style;
  {
    const char *decl = "font-weight:normal; color:currentColor; background-color:currentColor; visibility:hidden; --term-dim:on";
    d = mulib_css_parse_declarations(decl,strlen(decl),NULL);
  }
  assert(d); parent = style;
  assert(!mulib_termstyle_from_css(d,&parent,&style,NULL));
  assert(style.foreground == parent.foreground && style.background == parent.foreground);
  assert(!(style.attributes & MULIB_TERM_BOLD) && (style.attributes & MULIB_TERM_DIM) && (style.attributes & MULIB_TERM_CONCEAL));
  mulib_css_style_destroy(d);
  d = mulib_css_parse_declarations("width:3px",9,NULL); assert(d);
  assert(mulib_termstyle_from_css(d,NULL,&style,&error) == MULIB_TERMSTYLE_ENOTSUP && error.message);
  mulib_css_style_destroy(d);
  mulib_terminfo_free(&info);
  {
    size_t before = output.length;
    assert(mulib_termstyle_emit(saved,&info,0,&output) == MULIB_TERMSTYLE_ENOTSUP && output.length == before);
    assert(mulib_termstyle_write(saved,NULL,0,"\xc0\x80",2,&output) == MULIB_TERMSTYLE_EINVAL && output.length == before);
  }
  mulib_terminfo_ansi(&info); saved.attributes = 0; saved.foreground = MULIB_TERM_RGB(255,0,0);
  assert(!mulib_termstyle_emit(saved,&info,0,&output) && strstr(output.data,"\033[38;5;9m"));
  mulib_terminfo_free(&info); mulib_strbuf_free(&output);
  mulib_xml_destroy(node); mulib_css_destroy(sheet);
}
static void compare_caches(const mulib_termsyn_t *a, const mulib_termsyn_t *b) {
  size_t i;
  assert(mulib_termsyn_span_count(a) == mulib_termsyn_span_count(b));
  for (i = 0; i < mulib_termsyn_span_count(a); ++i) {
    mulib_termsyn_span_t x, y;
    assert(!mulib_termsyn_span(a,i,&x) && !mulib_termsyn_span(b,i,&y));
    assert(x.offset == y.offset && x.length == y.length && x.token == y.token);
  }
}
static void incremental(void) {
  const char *source = "int x = 1;\n/* block\n comment */\nreturn x;\n// end\n";
  const char *theme_source = "code.c {color:#eee} code.c .keyword {color:#f00; font-weight:bold} code .comment {font-style:italic; color:#080}";
  mulib_edbuf_t *b = mulib_edbuf_create(MULIB_EDBUF_ROPE,source,strlen(source));
  mulib_termsyn_t *h = mulib_termsyn_create(MULIB_TERMSYN_C), *fresh;
  mulib_termsyn_span_t span;
  mulib_term_style_t theme[MULIB_TERMSYN_TOKEN_COUNT];
  mulib_css_sheet_t *css = mulib_css_parse(theme_source,strlen(theme_source),NULL);
  mulib_strbuf_t out;
  size_t i, end = 0;
  assert(b && h && css && !mulib_termsyn_update(h,b));
  assert(mulib_termsyn_lexed_lines(h) == mulib_edbuf_line_count(b));
  assert(!mulib_termsyn_at(h,0,&span) && span.token == MULIB_TERMSYN_TYPE);
  assert(!mulib_termsyn_at(h,20,&span) && span.token == MULIB_TERMSYN_COMMENT);
  for (i = 0; i < mulib_termsyn_span_count(h); ++i) {
    assert(!mulib_termsyn_span(h,i,&span) && span.offset == end); end += span.length;
  }
  assert(end == strlen(source));
  assert(!mulib_termsyn_update(h,b) && mulib_termsyn_lexed_lines(h) == 0);
  assert(!mulib_edbuf_replace(b,8,1,"42",2) && !mulib_termsyn_update(h,b));
  assert(mulib_termsyn_lexed_lines(h) == 1 && mulib_termsyn_revision(h) == mulib_edbuf_revision(b));
  /* Remove comment opener: the next two lines change lexical entry states. */
  assert(!mulib_edbuf_erase(b,12,2) && !mulib_termsyn_update(h,b));
  assert(mulib_termsyn_lexed_lines(h) >= 2);
  fresh = mulib_termsyn_create(MULIB_TERMSYN_C); assert(fresh && !mulib_termsyn_update(fresh,b));
  compare_caches(h,fresh); mulib_termsyn_destroy(fresh);
  assert(!mulib_edbuf_insert(b,0,"// inserted\n",12) && !mulib_termsyn_update(h,b));
  assert(mulib_termsyn_lexed_lines(h) == 1);
  fresh = mulib_termsyn_create(MULIB_TERMSYN_C); assert(fresh && !mulib_termsyn_update(fresh,b));
  compare_caches(h,fresh); mulib_termsyn_destroy(fresh);
  assert(!mulib_termsyn_theme(css,MULIB_TERMSYN_C,theme,NULL));
  assert(theme[MULIB_TERMSYN_KEYWORD].foreground == MULIB_TERM_RGB(255,0,0));
  assert(theme[MULIB_TERMSYN_KEYWORD].attributes & MULIB_TERM_BOLD);
  assert(theme[MULIB_TERMSYN_TEXT].foreground == MULIB_TERM_RGB(238,238,238));
  mulib_strbuf_init(&out);
  assert(!mulib_termsyn_render(h,0,mulib_edbuf_size(b),theme,NULL,MULIB_TERMSTYLE_PLAIN,&out));
  { char *text = mulib_edbuf_copy(b); assert(text && out.length == strlen(text) && !memcmp(text,out.data,out.length)); free(text); }
  mulib_strbuf_free(&out); mulib_css_destroy(css); mulib_termsyn_destroy(h); mulib_edbuf_destroy(b);
}
static int custom_lexer(const char *s, size_t n, uint64_t state, mulib_termsyn_emit_fn emit,
                         void *ctx, uint64_t *next, void *user) {
  int *bad = (int *)user;
  (void)s; *next = state + 1;
  if (*bad && n) { (void)emit(ctx,n,1,MULIB_TERMSYN_STRING); return 1; }
  return !n || emit(ctx,0,n,MULIB_TERMSYN_KEYWORD);
}
static void languages(void) {
  static const char *sources[] = {"plain\n", "#define X 1\\\n +2\n'bad\n", "class A { bool x = true; };", "def f():\n  '''hello\n  world'''\n  return True\n", "{\"key\":true,\"n\":1.25e-3}", "if true; then\n echo \"hello\nworld\" $NAME # comment\nfi\n"};
  int lang;
  for (lang = 0; lang <= MULIB_TERMSYN_SHELL; ++lang) {
    mulib_edbuf_t *b = mulib_edbuf_create(MULIB_EDBUF_GAP,sources[lang],strlen(sources[lang]));
    mulib_termsyn_t *h = mulib_termsyn_create((mulib_termsyn_language_t)lang);
    mulib_termsyn_t *full;
    size_t step;
    assert(b && h && !mulib_termsyn_update(h,b));
    for (step = 0; step < 30; ++step) {
      size_t at = step * 17 % (mulib_edbuf_size(b)+1);
      assert(!mulib_edbuf_insert(b,at,step % 2 ? "\n" : "'",1));
      assert(!mulib_termsyn_update(h,b));
      full = mulib_termsyn_create((mulib_termsyn_language_t)lang);
      assert(full && !mulib_termsyn_update(full,b)); compare_caches(h,full); mulib_termsyn_destroy(full);
    }
    mulib_termsyn_destroy(h); mulib_edbuf_destroy(b);
  }
  {
    int bad = 0;
    mulib_edbuf_t *b = mulib_edbuf_create(MULIB_EDBUF_PIECE_TABLE,"abc",3);
    mulib_termsyn_t *h = mulib_termsyn_create_custom(custom_lexer,&bad);
    mulib_termsyn_span_t span;
    assert(h && b && !mulib_termsyn_update(h,b)); bad = 1;
    assert(!mulib_edbuf_insert(b,0,"z",1));
    assert(mulib_termsyn_update(h,b) == MULIB_TERMSYN_ELEXER && mulib_termsyn_revision(h) == 0);
    assert(!mulib_termsyn_span(h,0,&span) && span.length == 3);
    mulib_termsyn_destroy(h); mulib_edbuf_destroy(b);
  }
}
static void colored_backend(void) {
  const char *source = "return 42;";
  const char *css_text = ".keyword {color:#f00; font-weight:bold}";
  mulib_edbuf_t *b = mulib_edbuf_create(MULIB_EDBUF_PIECE_TABLE,source,strlen(source));
  mulib_termsyn_t *h = mulib_termsyn_create(MULIB_TERMSYN_C);
  mulib_css_sheet_t *css = mulib_css_parse(css_text,strlen(css_text),NULL);
  mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT];
  mulib_terminfo_t info = {0};
  mulib_vterm_t terminal;
  mulib_strbuf_t output = {NULL,0,0};
  const mulib_term_cell_t *cell;
  assert(b && h && css && !mulib_termsyn_update(h,b));
  assert(!mulib_termsyn_theme(css,MULIB_TERMSYN_C,styles,NULL));
  mulib_terminfo_ansi(&info);
  assert(!mulib_termsyn_render(h,0,mulib_edbuf_size(b),styles,&info,MULIB_TERMSTYLE_TRUECOLOR,&output));
  assert(mulib_vterm_init(&terminal,20,2) && mulib_vterm_feed(&terminal,output.data,output.length));
  cell = mulib_vterm_cell(&terminal,0,0);
  assert(cell && cell->codepoint == 'r' && cell->style.foreground == MULIB_TERM_RGB(255,0,0) && (cell->style.attributes & MULIB_TERM_BOLD));
  cell = mulib_vterm_cell(&terminal,7,0);
  assert(cell && cell->codepoint == '4' && cell->style.foreground == MULIB_TERM_COLOR_DEFAULT && !cell->style.attributes);
  mulib_strbuf_free(&output); mulib_vterm_free(&terminal); mulib_terminfo_free(&info);
  mulib_css_destroy(css); mulib_termsyn_destroy(h); mulib_edbuf_destroy(b);
}
int main(void) {
  styling(); incremental(); languages(); colored_backend();
  puts("terminal editor: CSS cascade, safe rendering and incremental lexers passed"); return 0;
}
