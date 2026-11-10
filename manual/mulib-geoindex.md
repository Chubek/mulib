# geoindex — spherical point lookup

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_GEOINDEX`; link `-lm`.

Add copies latitude/longitude in degrees, replacing an existing ID. Coordinates
must be finite, latitude within [-90,90], longitude within [-180,180]. Return
1 on success, 0 on invalid/allocation failure, preserving entries on failure.
Distance uses a spherical Earth radius of 6371 km and returns NaN for invalid
coordinates; it is not an ellipsoidal geodesic. Radius queries accept finite
nonnegative kilometers and include the boundary. Results follow entry order;
`SIZE_MAX` indicates invalid/capacity failure without output writes. Exact scans
cost O(points), with no query allocation; `free` releases storage.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-geoindex.h`](../include/mulib-geoindex.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Exact spherical great-circle distance and radius search over owned geographic
 * points. C11 single-header; define MULIB_IMPL_GEOINDEX once. */
#ifndef MULIB_GEOINDEX_H
#define MULIB_GEOINDEX_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    uint64_t id;
    double latitude, longitude;
  } mulib_geoindex_point_t;
  typedef struct
  {
    mulib_geoindex_point_t *points;
    size_t count, capacity;
  } mulib_geoindex_t;
  int mulib_geoindex_add (mulib_geoindex_t *g, uint64_t id, double latitude,
                          double longitude);
  double mulib_geoindex_distance_km (double lat1, double lon1, double lat2,
                                     double lon2);
  size_t mulib_geoindex_radius (const mulib_geoindex_t *g, double lat, double lon,
                                double radius_km, uint64_t *ids, size_t capacity);
  void mulib_geoindex_free (mulib_geoindex_t *g);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-geoindex.h`](../include/mulib-geoindex.h) |
| Implementation switch | `MULIB_IMPL_GEOINDEX` |
| Repository-local includes | None |
| Complete example | [`examples/modules/geoindex.c`](../examples/modules/geoindex.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/geoindex.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
