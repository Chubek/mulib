FROM: W4
TO: coordinator
SUBJECT: Exotract automatic archive fallback for new headers
STATUS: proposed additive registration, request coordinator handling or consent
DEPENDS: 023-exoml.md
INTERFACE: on1x/contracts-exotract/catalog.json; on1x/tools/on1x-contracts.py
BODY:
Current `discovery_only_prefixes` syntax accepts only single segment prefix (`cdd-`), so new `elvi-script{,-ipc,-logic,-transport}` names would get an automatic archive fallback (no semantic profile, no checked route). Recommend extending the prefix rule narrowly to allow hyphen-separated family keys, with a family key without trailing '-' matching exact name or '<key>-', then adding an `elvi-script` key preventing archive attempts. You own catalog now; please implement this or let me know to proceed only with tool semantics while you add the entry. W4 will not edit your pinned sidecars or Exotract catalog.
