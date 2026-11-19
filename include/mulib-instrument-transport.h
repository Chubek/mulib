/* MuLIB / Amb3r: instrument-transport. C11 single-header kernel.
 * Define MULIB_IMPL_INSTRUMENT_TRANSPORT in exactly one implementation translation unit.
 * Profile: Callback-based exact I/O; transport-specific drivers are caller-provided.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_INSTRUMENT_TRANSPORT_H
#define MULIB_INSTRUMENT_TRANSPORT_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {void *context;ptrdiff_t (*write)(void *,const unsigned char *,size_t);ptrdiff_t (*read)(void *,unsigned char *,size_t);} ma_instrument_transport;
int mulib_instrument_transport_write(ma_instrument_transport *t,const void *data,size_t n);
int mulib_instrument_transport_read_exact(ma_instrument_transport *t,void *data,size_t n);
int mulib_instrument_transport_read_line(ma_instrument_transport *t,char *out,size_t capacity,size_t *length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_INSTRUMENT_TRANSPORT) && !defined(MULIB_INSTRUMENT_TRANSPORT_H_IMPLEMENTED)
#define MULIB_INSTRUMENT_TRANSPORT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_instrument_transport_write(ma_instrument_transport *t,const void *data,size_t n){if(!t||!t->write||(!data&&n))return 0;const unsigned char *p=(const unsigned char*)data;size_t done=0;while(done<n){ptrdiff_t k=t->write(t->context,p+done,n-done);if(k<=0||(uintmax_t)k>n-done)return 0;done+=(size_t)k;}return 1;}
int mulib_instrument_transport_read_exact(ma_instrument_transport *t,void *data,size_t n){if(!t||!t->read||(!data&&n))return 0;unsigned char *p=(unsigned char*)data;size_t done=0;while(done<n){ptrdiff_t k=t->read(t->context,p+done,n-done);if(k<=0||(uintmax_t)k>n-done)return 0;done+=(size_t)k;}return 1;}
int mulib_instrument_transport_read_line(ma_instrument_transport *t,char *out,size_t capacity,size_t *length){if(!t||!out||!capacity||!length)return 0;size_t n=0;while(n+1<capacity){unsigned char c;if(!mulib_instrument_transport_read_exact(t,&c,1))return 0;if(c=='\n'){if(n&&out[n-1]=='\r')n--;out[n]=0;*length=n;return 1;}out[n++]=(char)c;}out[n]=0;return 0;}
#ifdef __cplusplus
}
#endif
#endif
