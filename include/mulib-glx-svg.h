/* MuLIB graphical extension: glx-svg. C11, header-only. */
#ifndef MULIB_GLX_SVG_H
#define MULIB_GLX_SVG_H
#include "mulib-glx-path.h"
#include <float.h>

#ifndef MULIB_GLX_SVG_MAX_BYTES
#define MULIB_GLX_SVG_MAX_BYTES 1048576u
#endif
#define MULIB_GLX_SVG_NUMBER_BYTES 128u

static inline int mulib_glx_svg_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline int mulib_glx_svg_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* Locale-independent SVG decimal grammar, not C strtod grammar: no hex,
 * infinity or NaN. Tokens are bounded, exponents cannot overflow an integer,
 * and the cursor/output are unchanged on failure. Tiny decimals may round to
 * zero. This is a coordinate parser, not a correctly-rounded decimal library. */
static inline int mulib_glx_svg_number_token(const char **cursor,
                                            const char *end, double *out)
{
    const char *s, *start;
    long double mantissa = 0, value;
    int negative = 0, digits = 0, significant = 0, scale = 0;
    int exponent = 0, exponent_negative = 0;
    if (!cursor || !*cursor || !end || !out)
        return 0;
    s = start = *cursor;
    if (s == end)
        return 0;
    if (*s == '+' || *s == '-') {
        negative = *s == '-';
        ++s;
    }
    for (int fractional = 0; fractional < 2; ++fractional) {
        while (s < end && mulib_glx_svg_digit(*s)) {
            int digit = *s++ - '0';
            if ((size_t)(s - start) > MULIB_GLX_SVG_NUMBER_BYTES)
                return 0;
            ++digits;
            if (significant < 19 && (digit || significant)) {
                mantissa = mantissa * 10 + digit;
                ++significant;
            } else if (significant == 19) {
                ++scale;
            }
            if (fractional)
                --scale;
        }
        if (fractional || s == end || *s != '.')
            break;
        ++s;
    }
    if (!digits)
        return 0;
    if (s < end && (*s == 'e' || *s == 'E')) {
        const char *exponent_start;
        ++s;
        if (s < end && (*s == '+' || *s == '-')) {
            exponent_negative = *s == '-';
            ++s;
        }
        exponent_start = s;
        while (s < end && mulib_glx_svg_digit(*s)) {
            if (exponent < 10000)
                exponent = exponent * 10 + (*s - '0');
            ++s;
            if ((size_t)(s - start) > MULIB_GLX_SVG_NUMBER_BYTES)
                return 0;
        }
        if (s == exponent_start)
            return 0;
    }
    if ((size_t)(s - start) > MULIB_GLX_SVG_NUMBER_BYTES)
        return 0;
    scale += exponent_negative ? -exponent : exponent;
    if (!significant) {
        value = 0;
    } else {
        /* Normalize first to avoid premature underflow on platforms where
         * long double has only double precision. */
        for (int i = 1; i < significant; ++i)
            mantissa /= 10;
        scale += significant - 1;
        if (scale > 308)
            return 0;
        if (scale < -324) {
            value = 0;
        } else if (scale < -300) {
            value = mantissa * powl(10, -300);
            value *= powl(10, scale + 300);
        } else {
            value = mantissa * powl(10, scale);
        }
    }
    if (negative)
        value = -value;
    if (!isfinite(value) || fabsl(value) > DBL_MAX)
        return 0;
    *out = (double)value;
    *cursor = s;
    return 1;
}

/* Legacy NUL-terminated number helper, retaining optional comma/whitespace. */
static inline int mgx_svg_number(const char **s, double *v)
{
    const char *cursor, *end;
    size_t n = 0;
    if (!s || !*s || !v)
        return 0;
    while (n < MULIB_GLX_SVG_MAX_BYTES && (*s)[n])
        ++n;
    end = *s + n;
    cursor = *s;
    while (cursor < end && mulib_glx_svg_space(*cursor))
        ++cursor;
    if (cursor < end && *cursor == ',') {
        ++cursor;
        while (cursor < end && mulib_glx_svg_space(*cursor))
            ++cursor;
    }
    if (!mulib_glx_svg_number_token(&cursor, end, v))
        return 0;
    *s = cursor;
    return 1;
}

/* Bounded SVG path-data subset: M/L/H/V/Q/C/Z, absolute/relative, implicit
 * repeated groups, and implicit lineto after moveto. Not an XML SVG reader.
 * The first command must be moveto. Append is atomic in logical state on
 * parse, geometry, resource-limit, or allocation failure. */
static inline int mulib_glx_svg_path_n(mulib_glx_path_t *p,
                                      const char *data, size_t length)
{
    const char *s, *end;
    char command = 0;
    mgx_point current = {0, 0}, saved_first;
    size_t saved_n;
    int saved_open, started = 0;
    if (!mulib_glx_path_storage_valid(p) || !data ||
        length > MULIB_GLX_SVG_MAX_BYTES)
        return 0;
    s = data;
    end = data + length;
    saved_n = p->n;
    saved_first = p->first;
    saved_open = p->open;
    while (s < end) {
        char c;
        int relative, explicit_command = 0, n;
        double v[6] = {0};
        while (s < end && mulib_glx_svg_space(*s))
            ++s;
        if (s == end)
            break;
        if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) {
            command = *s++;
            explicit_command = 1;
        }
        relative = command >= 'a' && command <= 'z';
        c = relative ? (char)(command - 'a' + 'A') : command;
        if (!started && c != 'M')
            goto fail;
        if (c == 'Z') {
            current = p->first;
            if (p->open && !mulib_glx_path_close(p))
                goto fail;
            command = 0;
            continue;
        }
        n = c == 'M' || c == 'L' ? 2 : c == 'H' || c == 'V' ? 1 :
            c == 'Q' ? 4 : c == 'C' ? 6 : 0;
        if (!n)
            goto fail;
        for (int i = 0; i < n; ++i) {
            while (s < end && mulib_glx_svg_space(*s))
                ++s;
            if (s < end && *s == ',' && (i || !explicit_command)) {
                ++s;
                while (s < end && mulib_glx_svg_space(*s))
                    ++s;
            }
            if (!mulib_glx_svg_number_token(&s, end, &v[i]))
                goto fail;
        }
        /* A segment following closepath starts at the closed subpath's
         * origin, rather than silently discarding that first segment. */
        if (c != 'M' && !p->open &&
            !mulib_glx_path_move(p, current.x, current.y))
            goto fail;
        if (c == 'H') {
            current.x = v[0] + (relative ? current.x : 0);
            if (!mulib_glx_path_line(p, current.x, current.y))
                goto fail;
        } else if (c == 'V') {
            current.y = v[0] + (relative ? current.y : 0);
            if (!mulib_glx_path_line(p, current.x, current.y))
                goto fail;
        } else {
            if (relative)
                for (int i = 0; i < n; i += 2) {
                    v[i] += current.x;
                    v[i + 1] += current.y;
                }
            if (c == 'M') {
                if (!mulib_glx_path_move(p, v[0], v[1]))
                    goto fail;
                command = relative ? 'l' : 'L';
                started = 1;
            } else if (c == 'L') {
                if (!mulib_glx_path_line(p, v[0], v[1]))
                    goto fail;
            } else if (c == 'Q') {
                if (!mulib_glx_path_quad(p, (mgx_point){v[0], v[1]},
                                        (mgx_point){v[2], v[3]}))
                    goto fail;
            } else if (!mulib_glx_path_cubic(p, (mgx_point){v[0], v[1]},
                                            (mgx_point){v[2], v[3]},
                                            (mgx_point){v[4], v[5]})) {
                goto fail;
            }
            current = (mgx_point){v[n - 2], v[n - 1]};
        }
    }
    return 1;
fail:
    p->n = saved_n;
    p->first = saved_first;
    p->open = saved_open;
    return 0;
}

static inline int mulib_glx_svg_path(mulib_glx_path_t *p, const char *s)
{
    size_t length = 0;
    if (!s)
        return 0;
    while (length < MULIB_GLX_SVG_MAX_BYTES && s[length])
        ++length;
    /* A string of exactly the limit is accepted via the explicit-length API.
     * The wrapper does not read one byte past its bounded scan. */
    if (length == MULIB_GLX_SVG_MAX_BYTES)
        return 0;
    return mulib_glx_svg_path_n(p, s, length);
}
#endif
