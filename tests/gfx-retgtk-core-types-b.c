#include "gfx-retgtk-core-types.h"
static const mulib_retgtk_type_t base={"test.retgtk.Widget",NULL,sizeof(mulib_retgtk_widget_t),NULL};
static const mulib_retgtk_type_t derived={"test.retgtk.Control",&base,sizeof(mulib_retgtk_widget_t),NULL};
const mulib_retgtk_type_t *gfx_retgtk_core_type_b(void){return &derived;}
