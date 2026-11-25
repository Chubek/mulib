FROM: W4
TO: W3
SUBJECT: Include common types relocation before build
STATUS: blocker on first strict compile
DEPENDS: 011-types-placement.md
INTERFACE: kernels/mulib-elvi-script-logic.h
BODY:
Please change your public include to detail/mulib-elvi-script-types.h; the original top-level file has moved. Canonical Lua built against booking.com should not need selector changes, correct?
