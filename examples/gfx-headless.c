/* Device-free shared renderer demo: cc -std=c11 -Ikernels
 * -Wno-misleading-indentation examples/gfx-headless.c -lm -ldl -o demo
 * ./demo [output.ppm] */
#include "mulib-immgtk-native.h"
#include "mulib-retgtk-box.h"
#include "mulib-retgtk-button.h"
#include "mulib-retgtk-label.h"
#include "mulib-glx-window.h"

static int
write_ppm(const mgx_surface *surface,const char *path)
{
 FILE *file=fopen(path,"wb");
 int okay;
 if(!file)return 0;
 okay=fprintf(file,"P6\n%d %d\n255\n",surface->w,surface->h)>0;
 for(int y=0;okay&&y<surface->h;y++){
  for(int x=0;x<surface->w;x++){
   uint32_t pixel=surface->pixels[(size_t)y*surface->stride+(size_t)x];
   unsigned char rgb[3]={(unsigned char)(pixel>>16),
                         (unsigned char)(pixel>>8),(unsigned char)pixel};
   if(fwrite(rgb,1,sizeof rgb,file)!=sizeof rgb){okay=0;break;}
  }
 }
 if(fclose(file)!=0)okay=0;
 return okay;
}

int
main(int argc,char **argv)
{
 mulib_glx_platform_t platform={0};
 mulib_glx_window_t window={0};
 mulib_glx_font_t font={0};
 mulib_immgtk_native_t immediate;
 mulib_retgtk_widget_t *box=NULL,*label=NULL,*button=NULL;
 mgx_surface *surface=NULL;
 int result=1;
 mulib_immgtk_native_init(&immediate);
 mulib_glx_platform_headless(&platform);
 if(!mulib_glx_font_builtin(&font)||
    !mulib_glx_window_init(&window,&platform,1,"MuLIB headless",400,180))
  goto done;
 surface=mulib_glx_swapchain_acquire(&window.swap);
 if(!surface)goto done;
 mgx_clear(surface,UINT32_C(0xff202630));
 immediate.font=&font;
 mulib_immgtk_native_begin(&immediate,surface,0,0,0,0);
 (void)mig_button(&immediate,"hello","Immediate MuLIB",
                  (mgx_rect){16,16,368,36},1);
 mulib_immgtk_native_end(&immediate);
 box=mulib_retgtk_box(1,1,8,8);
 label=mulib_retgtk_label(2,"Retained widgets, same pixels");
 button=mulib_retgtk_button(3,"Headless: no device access");
 if(!box||!label||!button||
    !mulib_retgtk_widget_add(box,label)||!mulib_retgtk_widget_add(box,button))
  goto done;
 mulib_retgtk_widget_arrange(box,(mgx_rect){8,64,384,104});
 {
  mret_paint paint={surface,&font,mgui_default_theme()};
  mulib_retgtk_widget_paint(box,&paint);
 }
 if(!mulib_glx_window_present(&window))goto done;
 if(argc>1&&!write_ppm(surface,argv[1]))goto done;
 result=0;
done:
 mulib_retgtk_widget_unref(label);
 mulib_retgtk_widget_unref(button);
 mulib_retgtk_widget_unref(box);
 mulib_immgtk_native_free(&immediate);
 mulib_glx_font_free(&font);
 mulib_glx_window_free(&window);
 return result;
}
