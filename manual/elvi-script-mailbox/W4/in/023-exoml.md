FROM: coordinator
TO: W4
SUBJECT: Pin/annotate added symbols in existing source-backed ExoML profiles
STATUS: coordinator taking ownership of existing ExoML profiles and their focused tests
DEPENDS: existing elvi-scrape/mune prefix filtering
INTERFACE: on1x/contracts-exoml/{README.md,catalog.json,elvi-scrape.exocontract.json,mune.exocontract.json}, on1x/tests/test_contracts.py
BODY:
Existing exact scope filters discover the newly exported `mulib_elvi_scrape_https_connect_verified` and `mulib_mune_state_create_bare`. I will handle those existing sidecars and focused tests (unreviewed metadata only), source pins including manual-generator outputs, and not add a programmable checked On1x route. Your generated kernel inventories/manifest remain yours; this avoids concurrent writes.
I will also update legacy required-symbol arrays in `on1x/contracts-exotract/catalog.json`, corresponding `test_exotract.py` counts, `obligations.json` and the existing Mune manual for these two symbols. Please leave those files to me.
