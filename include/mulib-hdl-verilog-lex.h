/** @file mulib-hdl-verilog-lex.h
 * @brief Source-offset tokens for identifiers, escaped identifiers, decimal/based-number-like
 * spans, punctuation and line/block comments.
 * @details C11 single-header implementation; define MULIB_IMPL_HDL_VERILOG_LEX in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_HDL_VERILOG_LEX_H
#define MULIB_HDL_VERILOG_LEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_hdl_tokkind {
    MULIB_HDL_EOF = 0,
    MULIB_HDL_ID = 1,
    MULIB_HDL_NUMBER = 2,
    MULIB_HDL_SYMBOL = 3,
    MULIB_HDL_ERROR = 4
} mulib_hdl_tokkind_t;
typedef struct mulib_hdl_token {
    mulib_hdl_tokkind_t kind;
    size_t offset, length;
} mulib_hdl_token_t;
typedef struct mulib_hdl_lexer {
    const char *text;
    size_t length, cursor;
} mulib_hdl_lexer_t;
void mulib_hdl_lexer_init(mulib_hdl_lexer_t *lex, const char *text, size_t len);
mulib_hdl_token_t mulib_hdl_lexer_next(mulib_hdl_lexer_t *lex);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HDL_VERILOG_LEX_H */
#if defined(MULIB_IMPL_HDL_VERILOG_LEX) && !defined(MULIB_HDL_VERILOG_LEX_H_IMPLEMENTED)
#define MULIB_HDL_VERILOG_LEX_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_hdl_lexer_init(mulib_hdl_lexer_t *l, const char *s, size_t n) {
    if (!l)
        return;
    l->text = s;
    l->length = s ? n : 0;
    l->cursor = 0;
}
static int mulib_hdl_ident_(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}
static int mulib_hdl_digit_(char c) { return c >= '0' && c <= '9'; }
mulib_hdl_token_t mulib_hdl_lexer_next(mulib_hdl_lexer_t *l) {
    mulib_hdl_token_t t = {MULIB_HDL_ERROR, 0, 0};
    if (!l || !l->text)
        return t;
    const char *s = l->text;
    size_t n = l->length, p = l->cursor;
    if (p > n)
        return t;
    for (;;) {
        while (p < n && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n'))
            p++;
        if (p + 1 < n && s[p] == '/' && s[p + 1] == '/') {
            p += 2;
            while (p < n && s[p] != '\n')
                p++;
            continue;
        }
        if (p + 1 < n && s[p] == '/' && s[p + 1] == '*') {
            p += 2;
            while (p + 1 < n && (s[p] != '*' || s[p + 1] != '/'))
                p++;
            if (p >= n || n - p < 2) {
                l->cursor = n;
                t.offset = p;
                t.length = n - p;
                return t;
            }
            p += 2;
            continue;
        }
        break;
    }
    l->cursor = p;
    t.offset = p;
    if (p == n) {
        t.kind = MULIB_HDL_EOF;
        return t;
    }
    char c = s[p++];
    if (c == '\\') {
        while (p < n && s[p] != ' ' && s[p] != '\t' && s[p] != '\r' && s[p] != '\n')
            p++;
        t.kind = p > t.offset + 1 ? MULIB_HDL_ID : MULIB_HDL_ERROR;
    } else if (mulib_hdl_ident_(c)) {
        while (p < n && (mulib_hdl_ident_(s[p]) || mulib_hdl_digit_(s[p])))
            p++;
        t.kind = MULIB_HDL_ID;
    } else if (mulib_hdl_digit_(c)) {
        while (p < n && (mulib_hdl_digit_(s[p]) || mulib_hdl_ident_(s[p]) || s[p] == '\''))
            p++;
        t.kind = MULIB_HDL_NUMBER;
    } else {
        t.kind = MULIB_HDL_SYMBOL;
    }
    t.length = p - t.offset;
    l->cursor = p;
    return t;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_HDL_VERILOG_LEX */
