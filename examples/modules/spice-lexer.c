/* Checked usage example for mulib-spice-lexer.h. */
#define MULIB_IMPL_SPICE_LEXER
#include "mulib-spice-lexer.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    double x;
    char fields[8][64];
    size_t n;
    assert(mulib_spice_parse_number("1MEG", &x) && x == 1000000);
    assert(mulib_spice_parse_number("2m", &x) && x == .002);
    assert(!mulib_spice_parse_number("2frob", &x));
    assert(mulib_spice_split_line("R1 in out 1k ; comment", fields, 8, &n) && n == 4 &&
           !strcmp(fields[0], "R1"));
    puts("PASS mulib-spice-lexer");
    return 0;
}
