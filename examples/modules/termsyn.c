#define MULIB_IMPL_EDBUF
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_TERMSYN
#include "mulib-termsyn.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  const char source[] = "int main(void) {\n  return 0; // hello\n}\n";
  const char css[] = "code.c {color:#ddd} .keyword {color:#c8f; font-weight:bold} .comment {color:#888; font-style:italic}";
  mulib_edbuf_t *buffer = mulib_edbuf_create(MULIB_EDBUF_ROPE,source,sizeof source-1);
  mulib_termsyn_t *syntax = mulib_termsyn_create(MULIB_TERMSYN_C);
  mulib_css_sheet_t *sheet = mulib_css_parse(css,sizeof css-1,NULL);
  mulib_term_style_t styles[MULIB_TERMSYN_TOKEN_COUNT];
  mulib_strbuf_t output = {NULL,0,0};
  assert(buffer && syntax && sheet && !mulib_termsyn_update(syntax,buffer));
  assert(!mulib_termsyn_theme(sheet,MULIB_TERMSYN_C,styles,NULL));
  assert(!mulib_edbuf_replace(buffer,26,1,"42",2));
  assert(!mulib_termsyn_update(syntax,buffer) && mulib_termsyn_lexed_lines(syntax) == 1);
  /* NULL capabilities deliberately select safe plain output. To color an
   * actual terminal, load an explicit terminfo record and pass &info instead.
   * Shared styles can also be passed directly to mulib_tui_text/curses_style. */
  assert(!mulib_termsyn_render(syntax,0,mulib_edbuf_size(buffer),styles,NULL,0,&output));
  assert(strstr(output.data,"return 42;")); fputs(output.data,stdout);
  mulib_strbuf_free(&output); mulib_css_destroy(sheet);
  mulib_termsyn_destroy(syntax); mulib_edbuf_destroy(buffer); return 0;
}
