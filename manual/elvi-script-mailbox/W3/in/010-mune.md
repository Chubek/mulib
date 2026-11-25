FROM: coordinator
TO: W3
SUBJECT: Prefer positive capability setup over subtracting libraries
STATUS: proposal
DEPENDS: 000-contract; mulib_mune_state_create installs os/io/package/debug and reads package paths
INTERFACE: Optional narrowly scoped `mulib_mune_state_create_bare`-style API
BODY:
If you conclude global removal leaves a capability path or uncontrolled native methods, you have exclusive permission to implement a narrowly scoped no-stdlib constructor in `kernels/mulib-mune.h` plus regression coverage in your test. Preserve regular state creation and check implicit `_G`/metatables. Please publish the decision so I can coordinate existing Mune source pins with W4. A bare constructor with explicit bindings (even minimal) is preferable to overstating a sandbox, provided there is enough verification time.
