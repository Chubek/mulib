/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_COMPLETION_H
#define MULIB_NB_COMPLETION_H
#include "mulib-wm-core.h"
typedef struct {const char *text;size_t replace_begin,replace_end;} mulib_nb_completion;
static inline int mulib_nb_ident_char(unsigned char x){return (x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x>='0'&&x<='9')||x=='_';}
/* ASCII identifier fallback over supplied symbol table; no claim of semantic completion. */
static inline size_t mulib_nb_complete(const char *src,size_t cursor,const char *const *symbols,size_t count,mulib_nb_completion *out,size_t capacity){if(!src||cursor>strlen(src)||(!symbols&&count)||(!out&&capacity))return 0;size_t start=cursor;while(start&&mulib_nb_ident_char((unsigned char)src[start-1]))start--;size_t n=0;for(size_t i=0;i<count&&n<capacity;i++)if(symbols[i]&&!strncmp(symbols[i],src+start,cursor-start)){int dup=0;for(size_t j=0;j<n;j++)if(!strcmp(out[j].text,symbols[i]))dup=1;if(!dup)out[n++]=(mulib_nb_completion){symbols[i],start,cursor};}return n;}
#endif
