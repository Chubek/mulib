/** @file mulib-texexpand.h
 * @brief Simple bounded macro-name substitution with single-pass escaped backslash commands.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_TEXEXPAND once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-texexpand.md for scope and limitations.
 */
#ifndef MULIB_TEXEXPAND_H
#define MULIB_TEXEXPAND_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_texexpand_macro { char name[64];char *replacement; } mulib_texexpand_macro_t;
typedef struct mulib_texexpand_engine { mulib_texexpand_macro_t *items;size_t count,capacity; } mulib_texexpand_engine_t;
int mulib_texexpand_define(mulib_texexpand_engine_t *engine,const char *name,const char *replacement);
int mulib_texexpand_once(const mulib_texexpand_engine_t *engine,const char *source,mulib_typecore_buf_t *out);
void mulib_texexpand_destroy(mulib_texexpand_engine_t *engine);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_TEXEXPAND_H */
#if defined(MULIB_IMPL_TEXEXPAND) && !defined(MULIB_TEXEXPAND_H_IMPLEMENTED)
#define MULIB_TEXEXPAND_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_texexpand_define(mulib_texexpand_engine_t *e,const char *name,const char *rep){if(!e||!name||!rep||!*name||strlen(name)>63)return 0;for(const char *p=name;*p;p++)if(!isalpha((unsigned char)*p))return 0;
 size_t n=strlen(rep);char *copy=(char*)malloc(n+1);if(!copy)return 0;memcpy(copy,rep,n+1);for(size_t i=0;i<e->count;i++)if(!strcmp(name,e->items[i].name)){free(e->items[i].replacement);e->items[i].replacement=copy;return 1;}
 if(e->count==e->capacity){size_t c=e->capacity?e->capacity*2:8;if(c>SIZE_MAX/sizeof(*e->items)){free(copy);return 0;}void *p=realloc(e->items,c*sizeof(*e->items));if(!p){free(copy);return 0;}e->items=(mulib_texexpand_macro_t*)p;e->capacity=c;}
 mulib_texexpand_macro_t *it=&e->items[e->count++];memset(it,0,sizeof(*it));memcpy(it->name,name,strlen(name)+1);it->replacement=copy;return 1;}
int mulib_texexpand_once(const mulib_texexpand_engine_t *e,const char *src,mulib_typecore_buf_t *out){if(!e||!src||!out)return 0;size_t n=strlen(src);for(size_t i=0;i<n;){if(src[i]=='\\'&&i+1<n&&isalpha((unsigned char)src[i+1])){size_t a=++i;while(i<n&&isalpha((unsigned char)src[i]))i++;size_t len=i-a;const char *rep=NULL;for(size_t j=0;j<e->count;j++)if(strlen(e->items[j].name)==len&&!memcmp(e->items[j].name,src+a,len)){rep=e->items[j].replacement;break;}if(rep){if(!mulib_typecore_text(out,rep))return 0;}else if(!mulib_typecore_byte(out,'\\')||!mulib_typecore_append(out,src+a,len))return 0;}else if(!mulib_typecore_byte(out,(unsigned char)src[i++]))return 0;}return 1;}
void mulib_texexpand_destroy(mulib_texexpand_engine_t *e){if(!e)return;for(size_t i=0;i<e->count;i++)free(e->items[i].replacement);free(e->items);memset(e,0,sizeof(*e));}
#ifdef __cplusplus
}
#endif
#endif
