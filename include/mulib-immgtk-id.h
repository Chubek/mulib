/* MuLIB graphical extension: immgtk-id. C11, header-only. */
#ifndef MULIB_IMMGTK_ID_H
#define MULIB_IMMGTK_ID_H
#include "mulib-immgtk-native.h"
static inline uint64_t mulib_immgtk_id(const mulib_immgtk_native_t *g,const char *s){return mig_id(g,s);}
static inline int mulib_immgtk_id_push(mulib_immgtk_native_t *g,const char *s){return mig_push_id(g,s);}
static inline int mulib_immgtk_id_pop(mulib_immgtk_native_t *g){return mig_pop_id(g);}
#endif
