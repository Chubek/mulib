/* MuLIB graphical extension: retgtk-calendar. C11, header-only. */
#ifndef MULIB_RETGTK_CALENDAR_H
#define MULIB_RETGTK_CALENDAR_H
#include "mulib-retgtk-grid.h"
#include "mulib-retgtk-button.h"
static inline int mulib_retgtk_calendar_days(int year,int month){static const int d[]={31,28,31,30,31,30,31,31,30,31,30,31};if(month<1||month>12||year<1||year>9999)return 0;return d[month-1]+(month==2&&year%4==0&&(year%100!=0||year%400==0));}
static inline int mulib_retgtk_calendar_weekday(int year,int month,int day){if(day<1||day>mulib_retgtk_calendar_days(year,month))return -1;static const int t[]={0,3,2,5,0,3,5,1,4,6,2,4};year-=month<3;return (year+year/4-year/100+year/400+t[month-1]+day)%7;}
static inline mulib_retgtk_widget_t *mulib_retgtk_calendar(uint64_t id,int year,int month){int n=mulib_retgtk_calendar_days(year,month);if(!n)return NULL;mulib_retgtk_widget_t *w=mulib_retgtk_grid(id,7,1);if(!w)return NULL;int offset=mulib_retgtk_calendar_weekday(year,month,1);for(int i=0;i<offset+n;i++){char s[8]="";if(i>=offset)snprintf(s,sizeof s,"%d",i-offset+1);mulib_retgtk_widget_t *c=mulib_retgtk_button(id+(uint64_t)i+1,s);if(!c||!mulib_retgtk_widget_add(w,c)){mulib_retgtk_widget_unref(c);mulib_retgtk_widget_unref(w);return NULL;}c->enabled=i>=offset;mulib_retgtk_widget_unref(c);}return w;}
#endif
