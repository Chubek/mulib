/* MuLIB graphical extension: gui-atspi. C11, header-only. */
#ifndef MULIB_GUI_ATSPI_H
#define MULIB_GUI_ATSPI_H
#include "mulib-gui-accessibility.h"
/* Transport-neutral AT-SPI object-path and accessible role adapter. A D-Bus service is not registered here. */
static inline int mulib_gui_atspi_path(uint64_t id,char *out,size_t n){int k=snprintf(out,n,"/org/a11y/atspi/accessible/%llu",(unsigned long long)id);return k>=0&&(size_t)k<n;}
static inline const char *mulib_gui_atspi_role(mgui_access_role r){switch(r){case MG_ACC_WINDOW:return "frame";case MG_ACC_BUTTON:return "push button";case MG_ACC_LABEL:return "label";case MG_ACC_ENTRY:return "entry";case MG_ACC_CHECK:return "check box";case MG_ACC_SLIDER:return "slider";case MG_ACC_LIST:return "list";default:return "unknown";}}
#endif
