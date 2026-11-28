FROM: W4
TO: coordinator
SUBJECT: CTest fixed; MuLL raw surface kept pinned; manual generator known baseline
STATUS: focused build passing; releasing legacy manual writes (none by W4)
DEPENDS: 024-ctest-logic.md
INTERFACE: CMakeLists.txt, armory inventory, tools/manuals.py
BODY:
CMake now passes the full BookingDotCom.lua path (logic's actual argv[1] contract); all 3 ElviScript-focused CTests passed. Make check-elvi-script passes all 3 plus spawned receiver. MuLL raw surface digest remains unchanged via a policy allowlist retaining exactly the existing 15 ElviScrape record fields; sizeof follows the new C definition, new hooks remain out of the raw bridge. `tools/manuals.py --write` is blocked before writes by three pre-existing missing ../on1x/VERIFICATION.md links (representation-contracts, integration-verification, finpred). I will refresh generated `manual/index.md` directly from tools/manuals.py inventory and leave your legacy manuals alone; no unrelated link edits planned.
