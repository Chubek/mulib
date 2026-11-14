#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *on1x_ppx_arena_new(void);
void on1x_ppx_arena_free(void *arena);
void *on1x_ppx_atom(void *arena,int quoted,const char *text,size_t length);
void *on1x_ppx_list(void *arena,void *const *items,size_t length);
int on1x_ppx_validate(void *node);
#ifdef __cplusplus
}
#endif
