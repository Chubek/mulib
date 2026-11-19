/* MuLIB / Amb3r: fpga-bitstream. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_BITSTREAM in exactly one implementation translation unit.
 * Profile: Architecture-neutral bit packing; no vendor-specific frames or programming file format.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_BITSTREAM_H
#define MULIB_FPGA_BITSTREAM_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_fpga_bitstream_set(unsigned char *bytes,size_t n,size_t offset,unsigned width,uint64_t value);
int mulib_fpga_bitstream_get(const unsigned char *bytes,size_t n,size_t offset,unsigned width,uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_BITSTREAM) && !defined(MULIB_FPGA_BITSTREAM_H_IMPLEMENTED)
#define MULIB_FPGA_BITSTREAM_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_bitstream_set(unsigned char *bytes,size_t n,size_t offset,unsigned width,uint64_t value){if(!bytes||!width||width>64||offset>SIZE_MAX-width||n>SIZE_MAX/8||offset+width>n*8)return 0;for(unsigned i=0;i<width;i++){size_t k=offset+i;unsigned char m=(unsigned char)(1u<<(k%8));if((value>>i)&1)bytes[k/8]|=m;else bytes[k/8]&=(unsigned char)~m;}return 1;}
int mulib_fpga_bitstream_get(const unsigned char *bytes,size_t n,size_t offset,unsigned width,uint64_t *out){if(!bytes||!out||!width||width>64||offset>SIZE_MAX-width||n>SIZE_MAX/8||offset+width>n*8)return 0;uint64_t v=0;for(unsigned i=0;i<width;i++){size_t k=offset+i;v|=(uint64_t)((bytes[k/8]>>(k%8))&1)<<i;}*out=v;return 1;}
#ifdef __cplusplus
}
#endif
#endif
