/* MuLIB graphical extension: retgtk-choice. C11, header-only. */
#ifndef MULIB_RETGTK_CHOICE_H
#define MULIB_RETGTK_CHOICE_H
#include "mulib-retgtk-button.h"
static inline mulib_retgtk_widget_t *mulib_retgtk_checkbox(uint64_t id,const char *s,int checked){return mulib_retgtk_toggle(id,s,checked);}
static inline int mulib_retgtk_choice_radio(mulib_retgtk_widget_t *group,size_t index){if(!group||index>=group->n)return 0;for(size_t i=0;i<group->n;i++){group->children[i]->value=i==index;mulib_retgtk_widget_invalidate(group->children[i]);}return 1;}
#endif
