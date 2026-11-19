/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_NB_STORE_H
#define MULIB_NB_STORE_H
#include "mulib-nb-serialize.h"
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
/* Atomic replacement in same directory; caller owns trusted directory. fsync parent separately for crash durability. */
static inline int mulib_nb_store_save(const char *path,const mulib_nb *n){if(!path||!*path||!n)return 0;size_t k=strlen(path);if(k>SIZE_MAX-12)return 0;char *tmp=(char*)malloc(k+12);if(!tmp)return 0;memcpy(tmp,path,k);memcpy(tmp+k,".tmp.XXXXXX",12);int fd=mkstemp(tmp);if(fd<0){free(tmp);return 0;}FILE *f=fdopen(fd,"wb");int ok=0;if(f){ok=mulib_nb_write(f,n)&&fflush(f)==0&&fsync(fd)==0;if(fclose(f)!=0)ok=0;}else close(fd);if(ok)ok=rename(tmp,path)==0;if(!ok)unlink(tmp);free(tmp);return ok;}
static inline int mulib_nb_store_load(const char *path,mulib_nb *out,size_t limit,size_t bytes){if(!path)return 0;FILE *f=fopen(path,"rb");if(!f)return 0;mulib_nb t={0};int ok=mulib_nb_read(f,&t,limit,bytes)&&fgetc(f)==EOF&&!ferror(f);fclose(f);if(!ok){mulib_nb_free(&t);return 0;}*out=t;return 1;}
#endif
