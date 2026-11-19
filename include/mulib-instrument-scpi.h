/* MuLIB / Amb3r: instrument-scpi. C11 single-header kernel.
 * Define MULIB_IMPL_INSTRUMENT_SCPI in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_INSTRUMENT_SCPI_H
#define MULIB_INSTRUMENT_SCPI_H
#include "mulib-amb3r-base.h"
#include "mulib-instrument-transport.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_instrument_scpi_command(ma_instrument_transport *t,const char *command);
int mulib_instrument_scpi_query(ma_instrument_transport *t,const char *command,char *response,size_t capacity,size_t *length);
int mulib_instrument_scpi_block(ma_instrument_transport *t,void *out,size_t cap,size_t *n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INSTRUMENT_SCPI) && !defined(MULIB_INSTRUMENT_SCPI_H_IMPLEMENTED)
#define MULIB_INSTRUMENT_SCPI_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_instrument_scpi_command(ma_instrument_transport *t,const char *command){if(!t||!command||!*command||strpbrk(command,"\r\n"))return 0;return mulib_instrument_transport_write(t,command,strlen(command))&&mulib_instrument_transport_write(t,"\n",1);}
int mulib_instrument_scpi_query(ma_instrument_transport *t,const char *command,char *response,size_t capacity,size_t *length){if(!command||!strchr(command,'?'))return 0;return mulib_instrument_scpi_command(t,command)&&mulib_instrument_transport_read_line(t,response,capacity,length);}
int mulib_instrument_scpi_block(ma_instrument_transport *t,void *out,size_t cap,size_t *n){unsigned char h[2];if(!t||!out||!n||!mulib_instrument_transport_read_exact(t,h,2)||h[0]!='#'||h[1]<'1'||h[1]>'9')return 0;size_t count=0;for(unsigned i=0;i<(unsigned)(h[1]-'0');i++){unsigned char c;if(!mulib_instrument_transport_read_exact(t,&c,1)||c<'0'||c>'9'||count>(SIZE_MAX-(c-'0'))/10)return 0;count=count*10+(c-'0');}*n=count;return count<=cap&&mulib_instrument_transport_read_exact(t,out,count);}
#ifdef __cplusplus
}
#endif
#endif
