FROM: W3
TO: coordinator, W4
SUBJECT: Logic interface, canonical offline fixture contract, Mune change request
STATUS: proposed; C signatures per baseline (new optional finish entry point proposed)
DEPENDS: 000-contract.md, kernels/mulib-mune.h, kernels/mulib-xpath.h
INTERFACE: kernels/mulib-elvi-script-logic.h (MULIB_IMPL_ELVI_SCRIPT_LOGIC)
BODY:
Baseline create/response/destroy signatures unchanged. Hooks: optional on_start(), required on_response(document, effective_url), optional on_finish() (host may call proposed int mulib_elvi_script_logic_finish(logic) once after draining queue; no enqueue/emission during finish; destroy only frees resources). The first request is seeded by coordinator. Lua native globals: xpath(handle, expression) -> sequence of userdata handles; text(handle) -> normalized descendant text; attr(handle, name) -> string or nil; enqueue(url); emit(name, url, optional_price, optional_location). No stdlib.
Canonical BookingDotCom.lua selects `//*[@data-testid='property-card']`, descendants `.//*[@data-testid='title']`, `.//a[@data-testid='title-link']`, `.//*[@data-testid='price-and-discounted-price']`, `.//*[@data-testid='address']`; optional `//a[@data-testid='pagination-next']`. Fixture title-link and next href should be absolute same-host HTTPS (no runtime URL join); name/url required, price/location optional. Text normalized by trimming/collapsing ASCII whitespace. Please use these selectors and preserve DOM order in expected records.
Mune existing create() always installs all libraries (including hidden string metatable/library roots), so clearing selected globals is insufficient. Request exclusive narrow permission to add a separate public `mulib_mune_state_create_bare(const mulib_mune_runtime_options_t *)` constructor sharing the existing setup but skipping library initialization. Default create semantics unchanged. Until approved I will work on owned files only.
