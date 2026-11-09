/** @file mulib-spice-netlist.h
 * @brief Transactional parser for four passive/independent-device card types: R, C, V, I with 2
 * nodes and one numeric DC value.
 * @details C11 single-header implementation; define MULIB_IMPL_SPICE_NETLIST in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_SPICE_NETLIST_H
#define MULIB_SPICE_NETLIST_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-spice-lexer.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_SPICE_MAX_NODES 128
#define MULIB_SPICE_MAX_PARTS 1024
typedef struct mulib_spice_part {
    char name[32];
    char kind;
    uint16_t a, b;
    double value;
} mulib_spice_part_t;
typedef struct mulib_spice_netlist {
    size_t nodes, parts;
    char node_names[MULIB_SPICE_MAX_NODES][32];
    mulib_spice_part_t parts_data[MULIB_SPICE_MAX_PARTS];
} mulib_spice_netlist_t;
int mulib_spice_netlist_parse(const char *source, mulib_spice_netlist_t *out, size_t *error_line);
int mulib_spice_netlist_find_node(const mulib_spice_netlist_t *net, const char *name);
int mulib_spice_netlist_valid(const mulib_spice_netlist_t *net);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SPICE_NETLIST_H */
#if defined(MULIB_IMPL_SPICE_NETLIST) && !defined(MULIB_SPICE_NETLIST_H_IMPLEMENTED)
#define MULIB_SPICE_NETLIST_H_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_netlist_find_node(const mulib_spice_netlist_t *net, const char *name) {
    if (!net || net->nodes > MULIB_SPICE_MAX_NODES || !name)
        return -1;
    if (!strcmp(name, "0") || !strcmp(name, "gnd"))
        return 0;
    for (size_t i = 0; i < net->nodes; i++)
        if (!strcmp(net->node_names[i], name))
            return (int)i + 1;
    return -1;
}
static int mulib_spice_node_get_(mulib_spice_netlist_t *n, const char *name) {
    int id = mulib_spice_netlist_find_node(n, name);
    if (id >= 0)
        return id;
    if (strlen(name) >= 32 || !name[0] || n->nodes >= MULIB_SPICE_MAX_NODES)
        return -1;
    strcpy(n->node_names[n->nodes++], name);
    return (int)n->nodes;
}
int mulib_spice_netlist_parse(const char *source, mulib_spice_netlist_t *out, size_t *error_line) {
    if (!source || !out)
        return 0;
    mulib_spice_netlist_t *t = calloc(1, sizeof *t);
    if (!t)
        return 0;
    size_t line = 0;
    int ok = 1;
    const char *p = source;
    while (*p && ok) {
        line++;
        char s[512];
        size_t len = 0;
        while (p[len] && p[len] != '\n' && len < sizeof s)
            len++;
        if (len == sizeof s) {
            ok = 0;
            break;
        }
        memcpy(s, p, len);
        s[len] = 0;
        p += len;
        if (*p == '\n')
            p++;
        char f[8][64];
        size_t z = 0;
        const char *trim = s;
        while (*trim == ' ' || *trim == '\t')
            trim++;
        if (*trim == '*' || !*trim)
            continue;
        if (!mulib_spice_split_line(trim, f, 8, &z)) {
            ok = 0;
            break;
        }
        if (!z)
            continue;
        if (!strcmp(f[0], ".end") || !strcmp(f[0], ".END"))
            break;
        if (f[0][0] == '.' || z != 4 || strlen(f[0]) >= 32 || t->parts >= MULIB_SPICE_MAX_PARTS) {
            ok = 0;
            break;
        }
        char kind = f[0][0];
        if (kind >= 'a' && kind <= 'z')
            kind = (char)(kind - 32);
        if (kind != 'R' && kind != 'C' && kind != 'I' && kind != 'V') {
            ok = 0;
            break;
        }
        double val = 0;
        int a = mulib_spice_node_get_(t, f[1]), b = mulib_spice_node_get_(t, f[2]);
        if (a < 0 || b < 0 || !mulib_spice_parse_number(f[3], &val) || (kind == 'R' && val <= 0) ||
            (kind == 'C' && val < 0)) {
            ok = 0;
            break;
        }
        for (size_t i = 0; i < t->parts; i++)
            if (!strcmp(t->parts_data[i].name, f[0]))
                ok = 0;
        if (!ok)
            break;
        mulib_spice_part_t *c = &t->parts_data[t->parts++];
        strcpy(c->name, f[0]);
        c->kind = kind;
        c->a = (uint16_t)a;
        c->b = (uint16_t)b;
        c->value = val;
    }
    if (ok)
        *out = *t;
    else if (error_line)
        *error_line = line;
    free(t);
    return ok;
}
int mulib_spice_netlist_valid(const mulib_spice_netlist_t *n) {
    if (!n || n->nodes > MULIB_SPICE_MAX_NODES || n->parts > MULIB_SPICE_MAX_PARTS)
        return 0;
    for (size_t i = 0; i < n->nodes; i++)
        if (!memchr(n->node_names[i], 0, sizeof n->node_names[i]) || !n->node_names[i][0])
            return 0;
    for (size_t i = 0; i < n->parts; i++) {
        const mulib_spice_part_t *p = n->parts_data + i;
        if (!memchr(p->name, 0, sizeof p->name) || !p->name[0] || p->a > n->nodes ||
            p->b > n->nodes || !isfinite(p->value))
            return 0;
        if (p->kind != 'R' && p->kind != 'C' && p->kind != 'I' && p->kind != 'V')
            return 0;
        if ((p->kind == 'R' && p->value <= 0) || (p->kind == 'C' && p->value < 0))
            return 0;
    }
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMPL_SPICE_NETLIST */
