/* MuLIB browser staging addition: locale-datetime
 * Implemented subset: Validated Gregorian ISO calendar date formatting.
 * Remaining: Locale calendars, timezones, patterns and CLDR.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_LOCALE_DATETIME_H
#define MULIB_LOCALE_DATETIME_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_locale_datetime_date(int y,unsigned m,unsigned d,char *out,size_t cap){static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(!out||y<0||y>9999||m<1||m>12||!d||d>days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0))||cap<11)return 0;char b[11];if(snprintf(b,sizeof b,"%04d-%02u-%02u",y,m,d)!=10)return 0;memcpy(out,b,11);return 1;}
#endif /* MULIB_LOCALE_DATETIME_H */
