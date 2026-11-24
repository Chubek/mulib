/** @file mulib-bcj.h
 * @brief Reversible XZ branch/call filters for x86, PowerPC, IA64, ARM, Thumb, SPARC.
 * Define MULIB_IMPL_BCJ. No allocation; trailing partial instructions unchanged.
 */
#ifndef MULIB_BCJ_H
#define MULIB_BCJ_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief Transform a whole block in place, XZ filter IDs 4..9. encode is 0
 * for inverse or 1 for forward. position must have the architecture's alignment.
 * Return zero, -1 for invalid arguments, or -5 for an unsupported ID. */
int mulib_bcj_transform (unsigned id, void *data, size_t size, uint32_t position, int encode);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BCJ) && !defined(MULIB_BCJ_IMPLEMENTED)
#define MULIB_BCJ_IMPLEMENTED
static uint32_t mulib_bcj_le_ (const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t mulib_bcj_be_ (const unsigned char *p)
{ return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static int mulib_bcj_sign_ (unsigned b) { return b==0 || b==255; }
int mulib_bcj_transform (unsigned id, void *data, size_t size, uint32_t position, int encode)
{
  unsigned char *p=(unsigned char *)data; size_t i=0; unsigned alignment;
  if (id<4 || id>9) return -5;
  alignment=id==4?1:id==6?16:id==8?2:4;
  if ((!data && size) || position%alignment || (encode!=0 && encode!=1)) return -1;
  if (id==4) {
    const unsigned char allowed[8]={1,1,1,0,1,0,0,0},shift[8]={0,1,2,2,3,3,3,3};
    unsigned mask=0; size_t previous=SIZE_MAX;
    while (size-i>=5) {
      size_t gap; uint32_t src,dst,pc;
      if ((p[i]&254)!=232) { ++i; continue; }
      gap=i-previous;
      if (gap>3) mask=0;
      else {
        mask=(mask<<(unsigned)(gap-1))&7;
        if (mask && (!allowed[mask] || mulib_bcj_sign_ (p[i+4-shift[mask]]))) {
          previous=i; mask=((mask<<1)|1)&7; ++i; continue;
        }
      }
      previous=i;
      if (!mulib_bcj_sign_ (p[i+4])) { mask=((mask<<1)|1)&7; ++i; continue; }
      src=mulib_bcj_le_ (p+i+1); pc=position+(uint32_t)i+5;
      for (;;) {
        unsigned bits;
        dst=encode?src+pc:src-pc;
        if (!mask) break;
        bits=shift[mask]*8;
        if (!mulib_bcj_sign_ ((dst>>(24-bits))&255)) break;
        src=dst^(((uint32_t)1<<(32-bits))-1);
      }
      p[i+1]=(unsigned char)dst; p[i+2]=(unsigned char)(dst>>8); p[i+3]=(unsigned char)(dst>>16);
      p[i+4]=(unsigned char)(0u-((dst>>24)&1u)); i+=5;
    }
    return 0;
  }
  if (id==6) {
    const unsigned char slots[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,6,6,0,0,7,7,4,4,0,0,4,4,0,0};
    for (;size-i>=16;i+=16) {
      unsigned slot,mask=slots[p[i]&31];
      for (slot=0;slot<3;++slot) if (mask&(1u<<slot)) {
        unsigned bit=5+41*slot,byte=bit/8,shift=bit&7,k; uint64_t chunk=0,instruction,fieldmask;
        uint32_t src,dst,pc=position+(uint32_t)i;
        for (k=0;k<6;++k) chunk|=(uint64_t)p[i+byte+k]<<(8*k);
        instruction=chunk>>shift;
        if (((instruction>>37)&15)!=5 || ((instruction>>9)&7)) continue;
        src=(uint32_t)(((instruction>>13)&0xfffff)|((instruction>>36)&1)<<20)<<4;
        dst=(encode?src+pc:src-pc)>>4;
        instruction&=~((UINT64_C(0xfffff)<<13)|(UINT64_C(1)<<36));
        instruction|=(uint64_t)(dst&0xfffff)<<13|(uint64_t)(dst&0x100000)<<16;
        fieldmask=((UINT64_C(1)<<41)-1)<<shift;
        chunk=(chunk&~fieldmask)|((instruction<<shift)&fieldmask);
        for (k=0;k<6;++k) p[i+byte+k]=(unsigned char)(chunk>>(8*k));
      }
    }
    return 0;
  }
  for (;size-i>=4;i+=alignment) {
    uint32_t src,dst,pc=position+(uint32_t)i;
    if (id==5) {
      src=mulib_bcj_be_ (p+i); if ((src&UINT32_C(0xfc000003))!=UINT32_C(0x48000001)) continue;
      dst=(encode?(src&0x03fffffc)+pc:(src&0x03fffffc)-pc)&0x03fffffc;
      dst|=src&UINT32_C(0xfc000003);
      p[i]=(unsigned char)(dst>>24); p[i+1]=(unsigned char)(dst>>16); p[i+2]=(unsigned char)(dst>>8); p[i+3]=(unsigned char)dst;
    } else if (id==7) {
      if (p[i+3]!=235) continue;
      src=((uint32_t)p[i]|(uint32_t)p[i+1]<<8|(uint32_t)p[i+2]<<16)<<2; pc+=8;
      dst=(encode?src+pc:src-pc)>>2;
      p[i]=(unsigned char)dst; p[i+1]=(unsigned char)(dst>>8); p[i+2]=(unsigned char)(dst>>16);
    } else if (id==8) {
      if ((p[i+1]&248)!=240 || (p[i+3]&248)!=248) continue;
      src=((uint32_t)(p[i+1]&7)<<19|(uint32_t)p[i]<<11|(uint32_t)(p[i+3]&7)<<8|p[i+2])<<1; pc+=4;
      dst=(encode?src+pc:src-pc)>>1;
      p[i]=(unsigned char)(dst>>11); p[i+1]=(unsigned char)(240|((dst>>19)&7));
      p[i+2]=(unsigned char)dst; p[i+3]=(unsigned char)(248|((dst>>8)&7)); i+=2;
    } else {
      src=mulib_bcj_be_ (p+i); if ((src>>22)!=0x100 && (src>>22)!=0x1ff) continue;
      src<<=2; dst=(encode?src+pc:src-pc)>>2;
      dst=((((0u-((dst>>22)&1u))<<22)|(dst&0x3fffff))&UINT32_C(0x3fffffff))|UINT32_C(0x40000000);
      p[i]=(unsigned char)(dst>>24); p[i+1]=(unsigned char)(dst>>16); p[i+2]=(unsigned char)(dst>>8); p[i+3]=(unsigned char)dst;
    }
  }
  return 0;
}
#endif
