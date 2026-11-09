/** @file mulib-netpolicy.h
 * @brief URL scheme and obvious loopback/private-host denial policy.
 * C11 single-header implementation. Define MULIB_IMPL_NETPOLICY in one translation unit.
 */
#ifndef MULIB_NETPOLICY_H
#define MULIB_NETPOLICY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Deny local hostnames, raw private/loopback IPv4 ranges, and non-HTTP(S) schemes. Not a DNS-resolution SSRF defense. */
int mulib_netpolicy_public_url(const char *url);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_NETPOLICY) && !defined(MULIB_IMPL_NETPOLICY_IMPLEMENTED)
#define MULIB_IMPL_NETPOLICY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_netpolicy_public_url(const char *s){if(!s)return 0;const char *p=NULL;if(!strncmp(s,"http://",7))p=s+7;else if(!strncmp(s,"https://",8))p=s+8;else return 0;char host[256];size_t i=0;while(p[i]&&p[i]!='/'&&p[i]!=':'&&p[i]!='?'&&p[i]!='#'&&i<255){host[i]=(char)tolower((unsigned char)p[i]);i++;}if(!i||i==255)return 0;host[i]=0;if(!strcmp(host,"localhost")||!strcmp(host,"0.0.0.0")||!strcmp(host,"::1")||!strcmp(host,"[::1]"))return 0;if(i>=6&&!strcmp(host+i-6,".local"))return 0;unsigned a,b,c,d;char end;if(sscanf(host,"%u.%u.%u.%u%c",&a,&b,&c,&d,&end)==4){if(a>255||b>255||c>255||d>255)return 0;if(a==0||a==10||a==127||a>=224||(a==169&&b==254)||(a==192&&b==168)||(a==172&&b>=16&&b<=31)||(a==100&&b>=64&&b<=127))return 0;}return 1;}

#ifdef __cplusplus
}
#endif
#endif
