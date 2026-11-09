/** @file mulib-eda-object.h
 * @brief Small immutable-ID object header with checked type/name, monotonic revision on rename;
 * object persistence and schema registry are outside scope.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_OBJECT in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_OBJECT_H
#define MULIB_EDA_OBJECT_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-ids.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_object {
    mulib_eda_id_t id;
    uint32_t revision, flags;
    uint16_t type;
    char name[64];
} mulib_eda_object_t;
int mulib_eda_object_init(mulib_eda_object_t *o, mulib_eda_id_t id, uint16_t type,
                          const char *name);
int mulib_eda_object_rename(mulib_eda_object_t *o, const char *name);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_OBJECT_H */
#if defined(MULIB_IMPL_EDA_OBJECT) && !defined(MULIB_EDA_OBJECT_H_IMPLEMENTED)
#define MULIB_EDA_OBJECT_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_object_init(mulib_eda_object_t *o, mulib_eda_id_t id, uint16_t type,
                          const char *name) {
    if (!o || !id || !type || !name || strlen(name) >= 64)
        return 0;
    memset(o, 0, sizeof *o);
    o->id = id;
    o->type = type;
    memcpy(o->name, name, strlen(name) + 1);
    o->revision = 1;
    return 1;
}
int mulib_eda_object_rename(mulib_eda_object_t *o, const char *name) {
    if (!o || !name || strlen(name) >= 64 || o->revision == UINT32_MAX)
        return 0;
    strcpy(o->name, name);
    o->revision++;
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_EDA_OBJECT */
