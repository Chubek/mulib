/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_MEMFD_H
#define MULIB_MEMFD_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if defined(__linux__) && defined(_GNU_SOURCE)
#include <sys/syscall.h>
#endif
/* Linux memfd where declared, otherwise an unlinked temporary file. No name survives success. */
static inline int mulib_memfd_create(const char *name,size_t size){int fd=-1;if(!name||!size||(uint64_t)size>INT64_MAX||(off_t)size<0||(uint64_t)(off_t)size!=(uint64_t)size){errno=EINVAL;return -1;}
#if defined(__linux__) && defined(_GNU_SOURCE)
fd=memfd_create(name,MFD_CLOEXEC|MFD_ALLOW_SEALING);
if(fd<0&&errno!=ENOSYS&&errno!=EINVAL)return -1;
#endif
if(fd<0){char path[]="/tmp/mulib-memfd-XXXXXX";fd=mkstemp(path);if(fd<0)return -1;if(unlink(path)<0){int e=errno;close(fd);errno=e;return -1;}if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0){int e=errno;close(fd);errno=e;return -1;}}if(ftruncate(fd,(off_t)size)<0){int e=errno;close(fd);errno=e;return -1;}return fd;}
static inline void *mulib_memfd_map(int fd,size_t size,int writable){if(fd<0||!size){errno=EINVAL;return MAP_FAILED;}struct stat st;if(fstat(fd,&st)<0)return MAP_FAILED;if(st.st_size<0||(uint64_t)st.st_size<size){errno=EINVAL;return MAP_FAILED;}return mmap(NULL,size,PROT_READ|(writable?PROT_WRITE:0),MAP_SHARED,fd,0);}
#endif
