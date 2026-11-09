/* Checked usage example for mulib-hdl-verilog-lex.h. */
#define MULIB_IMPL_HDL_VERILOG_LEX
#include "mulib-hdl-verilog-lex.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    const char *s = "module demo; // line\n wire x; /* block */ assign x=1'b1;";
    mulib_hdl_lexer_t l;
    mulib_hdl_lexer_init(&l, s, strlen(s));
    mulib_hdl_token_t t = mulib_hdl_lexer_next(&l);
    assert(t.kind == MULIB_HDL_ID && t.length == 6);
    t = mulib_hdl_lexer_next(&l);
    assert(t.kind == MULIB_HDL_ID && t.length == 4);
    int count = 2;
    while ((t = mulib_hdl_lexer_next(&l)).kind != MULIB_HDL_EOF) {
        assert(t.kind != MULIB_HDL_ERROR);
        assert(++count < 30);
    }
    assert(count > 7);
    puts("PASS mulib-hdl-verilog-lex");
    return 0;
}
