#ifndef ON1X_ARCHVIRT_STRING_H
#define ON1X_ARCHVIRT_STRING_H
#include <stddef.h>

/* Freestanding C lowering and compiler-emitted aggregate operations. */
void *memset(void *, int, size_t);
void *memcpy(void *restrict, const void *restrict, size_t);
void *memmove(void *, const void *, size_t);
int strcmp(const char *, const char *);
#endif
