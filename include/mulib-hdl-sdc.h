/* MuLIB / Amb3r: hdl-sdc. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_SDC in exactly one implementation translation unit.
 * Profile: Clock and I/O delay command generation only.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_SDC_H
#define MULIB_HDL_SDC_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hdl_sdc_clock_write(FILE *f,const char *name,const char *port,double period);
int mulib_hdl_sdc_delay_write(FILE *f,int output,const char *clock,const char *port,double delay);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_SDC) && !defined(MULIB_HDL_SDC_H_IMPLEMENTED)
#define MULIB_HDL_SDC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_sdc_clock_write(FILE *f,const char *name,const char *port,double period){if(!f||!ma_word(name)||!ma_word(port)||period<=0||!isfinite(period))return 0;return fprintf(f,"create_clock -name %s -period %.17g [get_ports %s]\n",name,period,port)>0;}
int mulib_hdl_sdc_delay_write(FILE *f,int output,const char *clock,const char *port,double delay){if(!f||!ma_word(clock)||!ma_word(port)||!isfinite(delay))return 0;return fprintf(f,"set_%s_delay -clock %s %.17g [get_ports %s]\n",output?"output":"input",clock,delay,port)>0;}
#ifdef __cplusplus
}
#endif
#endif
