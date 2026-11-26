# Offline PriceOpt qualification

This test emits an On1x application selecting a product price from a bounded
catalogue of candidate prices and expected unit sales. It tests **only**
`mulib.priceopt.candidates` in the existing reviewed, static KernelBind
profile. The operator-selected test key approves the profile's exact subject
(which also lists `optimize`); it is not a production review of either export.

Run from the MuLIB root with a local On1x compiler and a Lua-enabled Exotract
built with ExoML extraction. This needs native Linux AMD64, Clang resource
headers, a host C compiler, `ar` and OpenSSL (Ed25519).

```sh
python3 -B on1x/tests/integration/price_candidates/check.py \
  --on1xc /local/build/on1xc --exotract /local/build/exotract \
  --cc cc --artifacts /local/build/test-artifacts/price-candidates
```

The Python oracle evaluates the feasible rows with exact rational arithmetic,
independently of the native implementation, generated adapter and emitted
program. It checks profit and revenue, lower-price tie breaking, movement,
margin and tick constraints, nonzero slice offsets, the 256-element capacity
boundary and nominal `Err(Rejected())` for invalid or infeasible inputs. A
separately compiled C consumer links the same adapter object and archive,
checks the C declaration and result layout, and verifies that a real native
failure and adapter-side rejections leave output sentinels intact.

The input arrays are call-scoped, immutable snapshots. A copied value record
is the only successful result; no pointer, release operation or owner is
transferred to On1x. Changing the requested build or choosing the wrong
review authority prevents publication. Discovery, declaration/contract
checks, authorization, linking and oracle execution are distinct. Neither
metadata nor this finite test proves absence of retention, effects, or
undefined behavior in arbitrary foreign code. MIR/VM, load-time packages,
DSOs, unsupported exports and other platforms are outside the qualified
execution scope.
