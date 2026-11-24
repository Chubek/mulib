/** @file mulib-storage-core.h
 * @brief Checked allocation, owned byte storage and binary helpers.
 * C11 header-only substrate; standard library only.
 * Pointer/count arguments require live arrays of the declared lengths.
 * Stateful objects require caller synchronization.
 */
#ifndef MULIB_STORAGE_CORE_H
#define MULIB_STORAGE_CORE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Zero-initialize owned buffers; release only with the matching free function.
 * Capacities/counts are elements except bytes.n/cap, which are bytes.
 * Reserve/push may relocate storage. Appended sources must not overlap it.
 * Allocation and reserve use the C allocator; no shared compiled state.
 * For reserve/push, z is a nonzero element size, ptr/n/cap address writable
 * descriptors, n <= cap, and *ptr owns storage for cap elements (or is NULL
 * when cap is zero). Item addresses z readable, nonoverlapping bytes.
 * Size multiplication requires a writable out; overflow leaves it unchanged.
 * Alloc returns zeroed storage, including one byte for a zero-byte request.
 * Reserve leaves the descriptor unchanged on failure and may overallocate.
 * Identifier validation follows the current C locale.
 * Binary readers/writers use eight little-endian bytes per uint64_t. */
#define MULIB_STORAGE_NONE SIZE_MAX
#define MULIB_STORAGE_MIN(a,b) ((a)<(b)?(a):(b))
#define MULIB_STORAGE_MAX(a,b) ((a)>(b)?(a):(b))
typedef struct {unsigned char *v; size_t n,cap;} mulib_storage_bytes;
static inline int mulib_storage_size_mul(size_t a,size_t b,size_t *out){if(a && b>SIZE_MAX/a)return 0;*out=a*b;return 1;}
static inline void *mulib_storage_alloc(size_t n,size_t z){size_t b;if(!mulib_storage_size_mul(n,z,&b))return NULL;return calloc(b?b:1,1);}
static inline int mulib_storage_reserve(void **ptr,size_t *cap,size_t need,size_t z) {
 if(need<=*cap)return 1;
 size_t c=*cap?*cap:8,b;
 while(c<need){if(c>SIZE_MAX/2){c=need;break;}c*=2;}
 if(!mulib_storage_size_mul(c,z,&b))return 0;
 void *p=realloc(*ptr,b);
 if(!p)return 0;
 *ptr=p;*cap=c;return 1;
}
static inline int mulib_storage_push(void **p,size_t *n,size_t *cap,size_t z,const void *item){if(*n==SIZE_MAX||!mulib_storage_reserve(p,cap,*n+1,z))return 0;memcpy((unsigned char*)*p+*n*z,item,z);++*n;return 1;}
static inline int mulib_storage_bytes_append(mulib_storage_bytes *b,const void *src,size_t n){if(!b||(!src&&n)||n>SIZE_MAX-b->n)return 0;if(!mulib_storage_reserve((void**)&b->v,&b->cap,b->n+n,1))return 0;if(n)memcpy(b->v+b->n,src,n);b->n+=n;return 1;}
static inline void mulib_storage_bytes_free(mulib_storage_bytes *b){if(b){free(b->v);memset(b,0,sizeof *b);}}
static inline char *mulib_storage_string_dup(const char *s){if(!s)return NULL;size_t n=strlen(s);if(n==SIZE_MAX)return NULL;char *p=(char*)malloc(n+1);if(p)memcpy(p,s,n+1);return p;}
static inline int mulib_storage_identifier_valid(const char *s){if(!s||!(isalpha((unsigned char)*s)||*s=='_'))return 0;for(s++;*s;s++)if(!(isalnum((unsigned char)*s)||*s=='_'))return 0;return 1;}
static inline int mulib_storage_write_u64le(mulib_storage_bytes *b,uint64_t v){unsigned char x[8];for(int i=0;i<8;i++)x[i]=(unsigned char)(v>>(8*i));return mulib_storage_bytes_append(b,x,8);}
static inline int mulib_storage_read_u64le(const unsigned char *p,size_t n,size_t *pos,uint64_t *v){if(!p||!pos||!v||*pos>n||n-*pos<8)return 0;*v=0;for(int i=0;i<8;i++)*v|=(uint64_t)p[(*pos)++]<<(8*i);return 1;}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_STORAGE_CORE_H */
