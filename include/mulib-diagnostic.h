/* MuLIB compiler tools: Owned compiler diagnostics with severity, spans, and stable insertion order.
 * ISO C11 single-header module. Define MULIB_IMPL_DIAGNOSTIC in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_DIAGNOSTIC_H
#define MULIB_DIAGNOSTIC_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_DIAG_NOTE,MULIB_DIAG_WARNING,MULIB_DIAG_ERROR} mulib_diagnostic_severity_t;
typedef struct {int code;
mulib_diagnostic_severity_t severity;
size_t source_id,start,length;
char *message;
} mulib_diagnostic_t;
typedef struct {mulib_diagnostic_t *items;
size_t count,capacity,errors;
} mulib_diagnostic_list_t;
int mulib_diagnostic_add(mulib_diagnostic_list_t *d,int code,mulib_diagnostic_severity_t severity,size_t source,size_t start,size_t length,const char *message);
void mulib_diagnostic_free(mulib_diagnostic_list_t *d);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DIAGNOSTIC_H */
#if defined(MULIB_IMPL_DIAGNOSTIC) && !defined(MULIB_DIAGNOSTIC_IMPLEMENTED)
#define MULIB_DIAGNOSTIC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_diagnostic_free(mulib_diagnostic_list_t *d){if(!d)return;
for(size_t i=0;i<d->count;i++)free(d->items[i].message);
free(d->items);
memset(d,0,sizeof(*d));
}
int mulib_diagnostic_add(mulib_diagnostic_list_t *d,int code,mulib_diagnostic_severity_t sev,size_t src,size_t at,size_t len,const char *message){
 if(!d||!message||(unsigned)sev>MULIB_DIAG_ERROR||at>SIZE_MAX-len)return 0;
size_t n=strlen(message);
if(n==SIZE_MAX)return 0;
 char *s=(char*)malloc(n+1);
if(!s)return 0;
memcpy(s,message,n+1);
 if(d->count==d->capacity){size_t cap=d->capacity?d->capacity*2:8;
if(cap<d->capacity||cap>SIZE_MAX/sizeof(*d->items)){free(s);
return 0;
}void *p=realloc(d->items,cap*sizeof(*d->items));
if(!p){free(s);
return 0;
}d->items=(mulib_diagnostic_t*)p;
d->capacity=cap;
}
 d->items[d->count++]=(mulib_diagnostic_t){code,sev,src,at,len,s};
if(sev==MULIB_DIAG_ERROR)d->errors++;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
