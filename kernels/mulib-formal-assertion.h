/* MuLIB: formal-assertion. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_ASSERTION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_ASSERTION_H
#define MULIB_FORMAL_ASSERTION_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-geom2-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_formal_assertion(const unsigned char *trigger,const unsigned char *condition,size_t samples,size_t min_delay,size_t max_delay,size_t *first_failure);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_ASSERTION) && !defined(MULIB_FORMAL_ASSERTION_H_IMPLEMENTED)
#define MULIB_FORMAL_ASSERTION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_assertion(const unsigned char *trigger,const unsigned char *condition,size_t samples,size_t min_delay,size_t max_delay,size_t *first_failure){if((!trigger&&samples)||(!condition&&samples)||min_delay>max_delay)return -1;int pending=0;for(size_t i=0;i<samples;i++)if(trigger[i]){size_t end=MULIB_STORAGE_MIN(max_delay,samples-1-i);int found=0;for(size_t j=min_delay;j<=end;j++){if(condition[i+j]){found=1;break;}if(j==SIZE_MAX)break;}if(!found){if(max_delay>samples-1-i)pending=1;else{if(first_failure)*first_failure=i;return 0;}}}return pending?2:1;}
#ifdef __cplusplus
}
#endif
#endif
