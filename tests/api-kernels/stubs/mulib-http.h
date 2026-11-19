#ifndef MULIB_HTTP_H
#define MULIB_HTTP_H
#include <stddef.h>
typedef struct mulib_http_header {char *name,*value;} mulib_http_header_t;
typedef struct mulib_http_message {
int is_response;char *method,*target;unsigned status;char *reason;
mulib_http_header_t *headers;size_t header_count;unsigned char *body;size_t body_length;
int chunked,close_delimited;mulib_http_header_t *trailers;size_t trailer_count;
} mulib_http_message_t;
const char *mulib_http_header(const mulib_http_message_t *,const char *);
typedef int (*mulib_http_body_fn)(void *,const mulib_http_message_t *,const void *,size_t);
#endif
