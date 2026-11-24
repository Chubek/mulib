/** @file mulib-mesh-gltf.h
 * GLB 2.0 JSON and BIN chunk extraction.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: No glTF JSON/accessor/material import or export.
 */
#ifndef MULIB_MESH_GLTF_H
#define MULIB_MESH_GLTF_H
#include "mulib-mesh-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_media_bytes json,bin;} mulib_mesh_gltf_glb;
static inline uint32_t mulib_mesh_gltf_le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline bool mulib_mesh_gltf_parse_glb(mulib_media_bytes b,mulib_mesh_gltf_glb *out){if(!out||!b.data||b.size<20||mulib_mesh_gltf_le32(b.data)!=0x46546c67||mulib_mesh_gltf_le32(b.data+4)!=2||mulib_mesh_gltf_le32(b.data+8)!=b.size)return false;mulib_mesh_gltf_glb z={0};size_t p=12;while(p<b.size){if(b.size-p<8)return false;size_t n=mulib_mesh_gltf_le32(b.data+p);uint32_t t=mulib_mesh_gltf_le32(b.data+p+4);p+=8;if(n%4||n>b.size-p)return false;if(!z.json.data&&t!=0x4e4f534a)return false;if(t==0x4e4f534a){if(z.json.data||!n)return false;z.json.data=b.data+p;z.json.size=n;}else if(t==0x004e4942){if(z.bin.data)return false;z.bin.data=b.data+p;z.bin.size=n;}p+=n;}if(!z.json.data)return false;*out=z;return true;}
#ifdef __cplusplus
}
#endif
#endif
