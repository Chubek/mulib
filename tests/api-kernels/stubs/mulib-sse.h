#ifndef MULIB_SSE_H
#define MULIB_SSE_H
#include <stddef.h>
typedef struct mulib_sse_event {const char *event,*id,*data;size_t data_length;} mulib_sse_event_t;
typedef int (*mulib_sse_event_fn)(void*,const mulib_sse_event_t*);
typedef struct mulib_sse_parser mulib_sse_parser_t;
mulib_sse_parser_t *mulib_sse_create(size_t,mulib_sse_event_fn,void*);
int mulib_sse_feed(mulib_sse_parser_t*,const void*,size_t);
int mulib_sse_finish(mulib_sse_parser_t*);
void mulib_sse_destroy(mulib_sse_parser_t*);
#endif
