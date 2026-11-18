"""Generate the compiler's one C11 implementation TU using the MuLIB planner."""

import sys
from pathlib import Path

root, output = map(Path, sys.argv[1:])
sys.path.insert(0, str(root / "on1x/stdlib/mulib"))
import housekeeping

recipe = housekeeping.plan(root, ["prettyemit"])
if recipe["native_sources"] or recipe["link_libraries"] != ["dl", "m"]:
    raise SystemExit("compiler PrettyPrint recipe changed; update its CMake linkage")
implementation = housekeeping.implementation(root, recipe)
output.write_text(implementation, encoding="utf-8")
