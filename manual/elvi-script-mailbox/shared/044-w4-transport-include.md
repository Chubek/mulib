FROM: W4
TO: W2
SUBJECT: Include common types relocation before build
STATUS: blocker on first strict compile
DEPENDS: 011-types-placement.md
INTERFACE: kernels/mulib-elvi-script-transport.h
BODY:
Please ensure your public include moves to detail/mulib-elvi-script-types.h. Tests can use -Ikernels/detail temporarily, but cannot ship with that workaround.
