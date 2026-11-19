#include "link.h"
#include "mulib-immgtk-native.h"
#include "mulib-retgtk-button.h"

int
mulib_gfx_link_render(uint32_t *pixels,size_t count)
{
 mgx_surface surface={0};
 mulib_glx_font_t font={0};
 mulib_immgtk_native_t immediate;
 mulib_retgtk_widget_t *button;
 mret_paint paint;
 if(count<64*64||!mgx_wrap(&surface,pixels,64,64,64))return 0;
 if(!mulib_glx_font_builtin(&font))return 0;
 mulib_immgtk_native_init(&immediate);
 immediate.font=&font;
 mgx_clear(&surface,UINT32_C(0xff202630));
 mulib_immgtk_native_begin(&immediate,&surface,0,0,0,0);
 (void)mig_button(&immediate,"one","Immediate",(mgx_rect){2,2,60,24},1);
 mulib_immgtk_native_end(&immediate);
 mulib_immgtk_native_free(&immediate);
 button=mulib_retgtk_button(1,"Retained");
 if(!button){mulib_glx_font_free(&font);return 0;}
 paint=(mret_paint){&surface,&font,mgui_default_theme()};
 mulib_retgtk_widget_arrange(button,(mgx_rect){2,34,60,24});
 mulib_retgtk_widget_paint(button,&paint);
 mulib_retgtk_widget_unref(button);
 mulib_glx_font_free(&font);
 return 1;
}
