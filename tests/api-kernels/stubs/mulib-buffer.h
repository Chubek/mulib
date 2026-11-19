#ifndef MULIB_BUFFER_H
#define MULIB_BUFFER_H
#include <stddef.h>
typedef struct mulib_buffer { unsigned char *data; size_t length,capacity; } mulib_buffer_t;
int mulib_buffer_append(mulib_buffer_t*,const void*,size_t);
void mulib_buffer_free(mulib_buffer_t*);
#endif
