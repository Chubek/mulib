# layout

`distribute` divides a finite rectangle into one row or column by nonnegative
weights, inserting fixed gaps. Sizes/gaps must be nonnegative and finite;
nonempty weights require positive finite sum. Main-axis remaining space must
cover gaps. Zero weights receive zero main-axis length; empty input succeeds.

Returns 1 success, 0 invalid/overflow/allocation; output unchanged on failure,
including aliasing with input. Coordinates are double and subject to normal
rounding; this is not integer pixel snapping, CSS, wrapping or text layout.
Switch: `MULIB_IMPL_LAYOUT`. Example: `examples/modules/layout.c`.
