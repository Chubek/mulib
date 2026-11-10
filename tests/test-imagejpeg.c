/* Standalone API contract test for mulib-imagejpeg. */
#define MULIB_IMPL_TYPECORE
#define MULIB_IMPL_IMAGEJPEG
#include "mulib-imagejpeg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){unsigned char jpeg[]={0xff,0xd8,0xff,0xc0,0,7,8,0,2,0,3,0xff,0xd9};uint16_t w=0,h=0;assert(mulib_imagejpeg_dimensions(jpeg,sizeof(jpeg),&w,&h));assert(w==3&&h==2);return 0;}
