#ifndef MULIB_MUW3_H
#define MULIB_MUW3_H
#include "mulib-http.h"
#include <stddef.h>
typedef struct mulib_muw3_client {unsigned _dummy;} mulib_muw3_client_t;
typedef struct mulib_muw3_request { const char *url,*method;
const mulib_http_header_t *headers;size_t header_count;const void *body;size_t body_length; } mulib_muw3_request_t;
typedef struct mulib_muw3_response {mulib_http_message_t message;char *url;unsigned redirects;} mulib_muw3_response_t;
enum {MULIB_MUW3_OK=1};
int mulib_muw3_perform(const mulib_muw3_client_t*,const mulib_muw3_request_t*,mulib_muw3_response_t*);
int mulib_muw3_stream(const mulib_muw3_client_t*,const mulib_muw3_request_t*,mulib_http_body_fn,void*,mulib_muw3_response_t*);
void mulib_muw3_response_free(mulib_muw3_response_t*);
int mulib_muw3_form(const mulib_http_header_t*,size_t,size_t,char **);
#endif
