/* MuLIB: hdl-systemverilog-lex. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_SYSTEMVERILOG_LEX in exactly one implementation translation unit.
 * Profile: Basic span lexer, not full standards-conforming SystemVerilog tokenization.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_SYSTEMVERILOG_LEX_H
#define MULIB_HDL_SYSTEMVERILOG_LEX_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int (*mulib_hdl_systemverilog_lex_cb)(void *,unsigned,const char *,size_t,size_t);
int mulib_hdl_systemverilog_lex(const char *s,size_t n,mulib_hdl_systemverilog_lex_cb cb,void *ctx);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_SYSTEMVERILOG_LEX) && !defined(MULIB_HDL_SYSTEMVERILOG_LEX_H_IMPLEMENTED)
#define MULIB_HDL_SYSTEMVERILOG_LEX_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_systemverilog_lex(const char *s,size_t n,mulib_hdl_systemverilog_lex_cb cb,void *ctx){if((!s&&n)||!cb)return 0;size_t i=0;while(i<n){unsigned char c=(unsigned char)s[i];if(isspace(c)){i++;continue;}if(c=='/'&&i+1<n&&s[i+1]=='/'){i+=2;while(i<n&&s[i]!='\n')i++;continue;}if(c=='/'&&i+1<n&&s[i+1]=='*'){i+=2;while(i+1<n&&(s[i]!='*'||s[i+1]!='/'))i++;if(i+1>=n)return 0;i+=2;continue;}size_t start=i++;unsigned k=4;if(isalpha(c)||c=='_'){k=1;while(i<n&&(isalnum((unsigned char)s[i])||s[i]=='_'||s[i]=='$'))i++;}else if(isdigit(c)){k=2;while(i<n&&(isalnum((unsigned char)s[i])||s[i]=='_'||s[i]=='\''||s[i]=='.'))i++;}else if(c=='"'){k=3;int closed=0;while(i<n){if(s[i]=='\\'&&i+1<n){i+=2;continue;}if(s[i++]=='"'){closed=1;break;}}if(!closed)return 0;}if(!cb(ctx,k,s+start,i-start,start))return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
