/* MuLIB browser staging addition: html-script
 * Implemented subset: Script scheduling class for classic and module scripts.
 * Remaining: Preparation, MIME validation, blocking stylesheet interactions and actual execution.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_SCRIPT_H
#define MULIB_HTML_SCRIPT_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_html_script_PARSER_BLOCK,mulib_html_script_ASYNC,mulib_html_script_DEFER,mulib_html_script_IMMEDIATE} mulib_html_script_mode;
static inline mulib_html_script_mode mulib_html_script_classify(int parser_inserted,int external,int module,int async,int defer){if(async&&(external||module))return mulib_html_script_ASYNC;if(!parser_inserted)return mulib_html_script_IMMEDIATE;if(module||(external&&defer))return mulib_html_script_DEFER;return mulib_html_script_PARSER_BLOCK;}
#endif /* MULIB_HTML_SCRIPT_H */
