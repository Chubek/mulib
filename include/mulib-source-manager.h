/* MuLIB compiler tools: Owned source bytes with logarithmic offset-to-line and column mapping.
 * ISO C11 single-header module. Define MULIB_IMPL_SOURCE_MANAGER in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SOURCE_MANAGER_H
#define MULIB_SOURCE_MANAGER_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct {char *text;
size_t length;
size_t *line_starts;
size_t lines;
} mulib_source_manager_t;
int mulib_source_manager_set(mulib_source_manager_t *s,const void *bytes,size_t length);
int mulib_source_manager_position(const mulib_source_manager_t *s,size_t offset,size_t *line,size_t *column);
void mulib_source_manager_free(mulib_source_manager_t *s);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SOURCE_MANAGER_H */
#if defined(MULIB_IMPL_SOURCE_MANAGER) && !defined(MULIB_SOURCE_MANAGER_IMPLEMENTED)
#define MULIB_SOURCE_MANAGER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_source_manager_free(mulib_source_manager_t *s){if(s){free(s->text);
free(s->line_starts);
memset(s,0,sizeof(*s));
}}
int mulib_source_manager_set(mulib_source_manager_t *s,const void *b,size_t n){
 if(!s||(!b&&n)||n==SIZE_MAX)return 0;
 size_t count=1;
const unsigned char *p=(const unsigned char*)b;
 for(size_t i=0;i<n;i++)if(p[i]=='\n'){if(count==SIZE_MAX)return 0;
count++;
}
 if(count>SIZE_MAX/sizeof(size_t))return 0;
 char *copy=(char*)malloc(n+1);
size_t *starts=(size_t*)malloc(count*sizeof(size_t));
 if(!copy||!starts){free(copy);
free(starts);
return 0;
}
 if(n)memcpy(copy,b,n);
copy[n]=0;
starts[0]=0;
size_t j=1;
 for(size_t i=0;i<n;i++)if(p[i]=='\n')starts[j++]=i+1;
 mulib_source_manager_free(s);
s->text=copy;
s->length=n;
s->line_starts=starts;
s->lines=count;
return 1;
}
int mulib_source_manager_position(const mulib_source_manager_t *s,size_t offset,size_t *line,size_t *col){
 if(!s||!s->line_starts||offset>s->length||!line||!col)return 0;
 size_t lo=0,hi=s->lines;
while(lo+1<hi){size_t mid=lo+(hi-lo)/2;
if(s->line_starts[mid]<=offset)lo=mid;
else hi=mid;
}
 *line=lo+1;
*col=offset-s->line_starts[lo]+1;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
