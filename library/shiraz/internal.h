/* Private runtime ABI. Guest handles are indices, never host pointers. */
#ifndef SHIRAZ_INTERNAL_H
#define SHIRAZ_INTERNAL_H
#include "mulib-shiraz.h"
#include "mulib-kvdb.h"
enum { SHIRAZ_HANDLES = 256, SHIRAZ_TEXT = 1024 };
typedef struct shiraz_handle { FILE *file; char *key; } shiraz_handle;
struct mulib_shiraz_runtime {
    mulib_kvdb_t *registry;
    shiraz_handle handles[SHIRAZ_HANDLES];
    uint32_t last_error;
    int persistent;
};
typedef int (*shiraz_api)(mulib_shiraz_runtime_t *, mulib_shiraz_t *, const char *);
int shiraz_equal(const char *, const char *);
int shiraz_text(mulib_shiraz_t *, uint64_t, char *, size_t, int);
int shiraz_get(mulib_shiraz_t *, uint64_t, unsigned, uint64_t *);
int shiraz_put(mulib_shiraz_t *, uint64_t, unsigned, uint64_t);
int shiraz_args(mulib_shiraz_t *, uint64_t *, unsigned);
unsigned shiraz_handle_new(mulib_shiraz_runtime_t *);
shiraz_api shiraz_kernel32_find(const char *);
shiraz_api shiraz_advapi32_find(const char *);
int shiraz_registry_key(mulib_shiraz_runtime_t *, const char *, int);
#endif
