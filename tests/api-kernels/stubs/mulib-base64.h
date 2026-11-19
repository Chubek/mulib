#ifndef MULIB_BASE64_H
#define MULIB_BASE64_H
#include <stddef.h>
size_t mulib_base64_encode(const void*,size_t,char*,size_t);
int mulib_base64_decode(const char*,size_t,unsigned char*,size_t,size_t*);
#endif
