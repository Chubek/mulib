/* MuLIB graphical extension: immgtk-commandpalette. C11, header-only. */
#ifndef MULIB_IMMGTK_COMMANDPALETTE_H
#define MULIB_IMMGTK_COMMANDPALETTE_H
#include "mulib-immgtk-native.h"
static inline int mulib_immgtk_commandpalette_score(const char *label,const char *query){int score=0,run=0;for(;*label&&*query;label++){unsigned a=(unsigned char)*label,b=(unsigned char)*query;if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a==b){query++;score+=10+run++;}else{score--;run=0;}}return *query?INT_MIN:score;}
static inline int mulib_immgtk_commandpalette_best(const char *const *labels,size_t n,const char *query){int best=-1,score=INT_MIN;for(size_t i=0;i<n&&i<INT_MAX;i++){int s=mulib_immgtk_commandpalette_score(labels[i],query);if(s>score){score=s;best=(int)i;}}return best;}
#endif
