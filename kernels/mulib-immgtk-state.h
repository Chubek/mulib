/* MuLIB graphical extension: immgtk-state. C11, header-only. */
#ifndef MULIB_IMMGTK_STATE_H
#define MULIB_IMMGTK_STATE_H
#include "mulib-immgtk-native.h"
typedef mig_state mulib_immgtk_state_entry_t;
static inline mig_state *mulib_immgtk_state_find(mulib_immgtk_native_t *g,uint64_t id){return id?mig_state_get(g,id):NULL;}
#endif
