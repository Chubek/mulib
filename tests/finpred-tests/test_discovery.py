#!/usr/bin/env python3
"""Run real declaration discovery for all FinPRED modules; assert no admission."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


NAMES = (
    "contract ratequote staymatch compset pricecurve asof decisiontrace ruleeval "
    "backtest marketquote timeseries corporate-actions fxrate portfolio risk "
    "orderbook orderintent"
).split()
ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exotract", type=lambda s: Path(s).resolve(), required=True)
    parser.add_argument("--on1xc", type=lambda s: Path(s).resolve(), required=True)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--clang", default="clang")
    args = parser.parse_args()
    resource = subprocess.check_output([args.clang, "-print-resource-dir"], text=True).strip()
    command = [sys.executable, ROOT / "on1x/tools/on1x-build.py", "--no-prelude",
               "--mulib-root", ROOT, "--exotract", args.exotract, "--on1xc", args.on1xc,
               "--cc", args.cc, "--resource-dir", resource]

    def run(*argv):
        return subprocess.run([*command, *argv], text=True, capture_output=True, timeout=120)

    with tempfile.TemporaryDirectory(prefix="mulib-finpred-kernelbind-") as temporary:
        path = Path(temporary)
        source, report, denied = [path / name for name in ("finpred.on1x", "report.json", "denied")]
        aliases = {f"mulib.{name.replace('-', '_')}": name for name in NAMES}
        source.write_text("".join(f"import kernel {alias} as K{i};\n"
                                  for i, alias in enumerate(aliases)) + "fn main(){}\n")
        result = run("--inspect-kernels", source, "-o", report)
        assert result.returncode == 0, result.stderr
        data = json.loads(report.read_bytes())
        assert data["safe_export"] is False and set(data["modules"]) == set(aliases)
        for alias, name in aliases.items():
            module = data["modules"][alias]
            assert module["safe_export"] is False and module["callable"] is False, alias
            assert module["metadata"]["cdecl"]["complete"] and module["symbols"], alias
            assert Path(module["metadata"]["cdecl"]["header"]).name == f"mulib-{name}.h"
            assert all(not symbol["callable"] for symbol in module["symbols"].values()), alias
        for mode, prefix, diagnostic in (
            ("--kernel-bind", "", "ON1X-KERNEL-NOT-ADMITTED"),
            ("--kernel-bind", "unsafe ", "ON1X-KERNEL-UNSAFE-UNSUPPORTED"),
            ("--kernel-review-candidate", "", "ON1X-KERNEL-NOT-ADMITTED"),
        ):
            source.write_text(f"{prefix}import kernel mulib.staymatch;\nfn main(){{}}\n")
            result = run(mode, source, "-o", denied)
            assert result.returncode and diagnostic in result.stderr and not denied.exists(), result
    print("FinPRED: real CDecl discovery for all 17 kernels; callable admission remains closed.")


if __name__ == "__main__":
    main()
