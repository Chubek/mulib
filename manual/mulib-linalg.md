# mulib-linalg

Graphics math using copyable, allocation-free `float` vectors (`vec2`, `vec3`,
`vec4`), 4×4 matrices and `(x,y,z,w)` quaternions. Define `MULIB_IMPL_LINALG`
in one C11 translation unit and link `-lm`. Declarations have C linkage.

## Conventions

Matrices are column-major: `m[column*4 + row]`. Vectors are columns;
`mat4_mul(a,b)` applies `b` first. Translation occupies indices 12–14.
Coordinates are right-handed, angles are radians, the view camera looks down
negative Z. Perspective and orthographic projections use OpenGL NDC Z in
`[-1,1]`. The caller performs the homogeneous divide after `mat4_transform`.

Vector arithmetic, dot/cross products, lengths and interpolation return values.
They follow ordinary floating-point semantics; arithmetic can overflow.
`vec3_lerp` permits extrapolation. Normalization uses double intermediates so
finite subnormal and very large input vectors can be normalized without first
overflowing/underflowing a float squared length.

## Checked operations

Normalization, inverse, look-at, projection and quaternion conversion/interpolation
return **1 on success, 0 on failure**, preserving the output on failure. A NULL
output fails. Passing a value and its original output variable is supported.
Nonfinite or zero normalization inputs fail. Inverse uses double-precision
pivoted Gauss–Jordan elimination, rejecting singular matrices and inverses that
cannot be represented as finite floats. This is a numerical inverse, not an
exact singularity/conditioning test; ill-conditioned inputs can lose precision.

`look_at` rejects coincident eye/target and up parallel to the viewing direction.
Perspective requires `0<fovy<pi`, positive aspect, and `0<near<far`.
Orthographic bounds must be finite and increasing; near may be negative.
Both reject nonfinite results. Quaternion axis-angle normalizes its axis;
matrix conversion normalizes its quaternion. Hamilton multiplication composes
rotations right-to-left. Slerp normalizes endpoints, uses the shortest arc, and
requires `t` in `[0,1]`.

## Usage

```c
mulib_linalg_vec3_t offset = {2, 3, 4};
mulib_linalg_vec4_t vertex = {1, 0, 0, 1};
mulib_linalg_mat4_t model = mulib_linalg_mat4_translation(offset);
vertex = mulib_linalg_mat4_transform(model, vertex); /* {3,3,4,1} */
```

Build `examples/modules/linalg.c` with
`cc -std=c11 -Iinclude examples/modules/linalg.c -lm -o linalg`.
`tests/test_graphics_math.c` checks inverse roundtrips, projection endpoints,
quaternion rotations and normalization extremes. No hidden state or allocations.
