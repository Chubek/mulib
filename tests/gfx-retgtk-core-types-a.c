#include "gfx-retgtk-core-types.h"
static const mulib_retgtk_type_t base={"test.retgtk.Widget",NULL,sizeof(mulib_retgtk_widget_t),NULL};
static const mulib_retgtk_type_t derived={"test.retgtk.Control",&base,sizeof(mulib_retgtk_widget_t),NULL};
const mulib_retgtk_type_t *gfx_retgtk_core_type_a(void){return &derived;}
mulib_retgtk_widget_t *gfx_retgtk_core_widget_a(void){
    mulib_retgtk_widget_t *w=mret_control(123,"cross-TU",NULL);
    if(w&&!mulib_retgtk_widget_set_type(w,&derived)){mulib_retgtk_widget_unref(w);return NULL;}
    return w;
}
