/** @file mulib-scrape-table.h
 * @brief Simple HTML table cell counting for static document inspection.
 * C11 single-header implementation. Define MULIB_IMPL_SCRAPE_TABLE in one translation unit.
 */
#ifndef MULIB_SCRAPE_TABLE_H
#define MULIB_SCRAPE_TABLE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Count opening <td> and <th> tags, ASCII case insensitive; not a full HTML parser. */
size_t mulib_scrape_table_cells(const char *html);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SCRAPE_TABLE) && !defined(MULIB_IMPL_SCRAPE_TABLE_IMPLEMENTED)
#define MULIB_IMPL_SCRAPE_TABLE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_scrape_table_cells(const char *s){if(!s)return 0;size_t n=0;for(size_t i=0;s[i];i++)if(s[i]=='<'&&tolower((unsigned char)s[i+1])=='t'&&(tolower((unsigned char)s[i+2])=='d'||tolower((unsigned char)s[i+2])=='h')&&(s[i+3]=='>'||isspace((unsigned char)s[i+3])))n++;return n;}

#ifdef __cplusplus
}
#endif
#endif
