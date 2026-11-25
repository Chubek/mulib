# Energy budget

Check a greenhouse's fixed seven-day equipment schedule against a daily energy
allowance of 3,500 Wh. [`schedule.on1x`](src/schedule.on1x) supplies the run times;
[`main.on1x`](src/main.on1x) converts minutes to watt-hours and reports daily
use, weekly use, peak daily use, the number of over-budget days, and the sum of
the daily overruns. A day exactly at the allowance is not over budget. Spare
energy on other days does not cancel a daily overrun.

The pump draws 1,200 W, the fan 360 W, and the lights 240 W while running.
Dividing each power by 60 gives exactly 20, 6, and 4 Wh per minute. The sample
assumes constant draw and ignores startup and standby use. It does not read
meter data, accept new schedules at runtime, or estimate bills.

| Day | Pump min | Fan min | Lights min | Use (Wh) | Overrun (Wh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mon | 90 | 180 | 240 | 3,840 | 340 |
| Tue | 75 | 150 | 240 | 3,360 | 0 |
| Wed | 105 | 180 | 240 | 4,140 | 640 |
| Thu | 60 | 140 | 240 | 3,000 | 0 |
| Fri | 120 | 150 | 240 | 4,260 | 760 |
| Sat | 45 | 110 | 180 | 2,280 | 0 |
| Sun | 80 | 170 | 220 | 3,500 | 0 |

[`expected.txt`](expected.txt) lists the seven daily totals in order, followed
by `24380` (weekly Wh), `4260` (peak Wh), `3` (days over budget), and `1740`
(excess Wh). For example, Monday uses `(90 × 1200 + 180 × 360 + 240 × 240) / 60`
Wh. The weekly total is below `7 × 3500` Wh even though three individual days
exceed their cap.

## Build and check

From the MuLIB root, set `ON1XC` to an absolute path to a built compiler with
its matching `on1x-vm` beside it. Set `BUILD_ROOT` to a writable build directory
and `HOST_CC` to an explicit host C driver. The commands copy the sample to a
fresh stage and leave the checkout and compiler build untouched.

```sh
set -eu
: "${ON1XC:?set the absolute path to on1xc}"
: "${BUILD_ROOT:?set your writable build directory}"
: "${HOST_CC:?set the absolute path to a host C driver, e.g. /usr/bin/cc}"
mkdir -p "$BUILD_ROOT"
work=$(mktemp -d "$(cd "$BUILD_ROOT" && pwd)/energy-budget.XXXXXX")
cp -R on1x/examples/energy-budget "$work/project"
mkdir "$work/tmp"
export TMPDIR="$work/tmp" PYTHONDONTWRITEBYTECODE=1

for backend in c11 mir-c11 mir-vm; do
  out="$work/$backend"
  python3 -B on1x/tools/on1x-project.py build "$work/project" \
    --on1xc "$ON1XC" --backend "$backend" --cc "$HOST_CC" --output-dir "$out"
  "$out/energy-budget" >"$work/$backend.stdout" 2>"$work/$backend.stderr"
  cmp "$work/project/expected.txt" "$work/$backend.stdout"
  test ! -s "$work/$backend.stderr"
done

"$(dirname "$ON1XC")/on1x-vm" --verify "$work/mir-vm/energy-budget.o1vm"
"$ON1XC" --emit-ir "$work/project/src/main.on1x" -o "$work/energy-budget.mir"
"$ON1XC" --ir-input --run-ir "$work/energy-budget.mir" \
  >"$work/mir-eval.stdout" 2>"$work/mir-eval.stderr"
cmp "$work/project/expected.txt" "$work/mir-eval.stdout"
test ! -s "$work/mir-eval.stderr"
```

`--cc` selects host tools for the two C11 builds; the VM build produces
bytecode and a bundled runner without using the C driver. No kernel imports,
PPX, managed values, or file I/O are needed.

**Qualification:** On Linux x86-64 with GCC 16.2.1 as the explicit host C
driver and the MuLIB `167286b84830f019e3c19fc9d9eef8b9e3ce6c59` baseline
compiler and runner, all three project artifacts and the saved-MIR evaluator
exited 0, matched the expected stdout byte-for-byte, and emitted no stderr.
The standalone runner verified the project bytecode. The baseline did not
have the optional Archon provider; this example does not qualify Archon or
the default Cephyr project toolchain.
