#!/usr/bin/env bash
# Four serial real-MuLL runs, reversing both compiler and fixture order.
# Invoke from the repository root, only when measurement machines are quiet.
set -euo pipefail
if [[ $# != 5 ]]; then
  echo "usage: bash on1x/benchmark/zero-cost.sh HARNESS BASELINE CANDIDATE ARCHON FRESH_DIR" >&2
  exit 2
fi
harness=$(realpath "$1")
baseline=$(realpath "$2")
candidate=$(realpath "$3")
archon=$(realpath "$4")
mkdir "$5" # refuse reuse, even if a previous run was interrupted
out=$(realpath "$5")
sha256sum "$harness" "$baseline" "$candidate" "$archon" > "$out/tools.sha256"
run() {
  local tag=$1 compiler=$2 cases=$3
  date -u -Iseconds > "$out/$tag.start"
  ps -eo pid,comm,pcpu --sort=-pcpu > "$out/$tag.processes"
  "$harness" run --config on1x/benchmark/config/measurement.json \
    --compiler "$compiler" --archon "$archon" --cc /usr/bin/cc \
    --cflags '["-O2"]' --iterations 1000000 --seed 1729 --cases "$cases" \
    --output "$out/$tag"
  date -u -Iseconds > "$out/$tag.end"
}
# Include both previous diagnostics: five of their six executables changed.
pair=zero-explicit,zero-wrapped,processor-lazy,processor-identities
reverse=processor-identities,processor-lazy,zero-wrapped,zero-explicit
run first-baseline "$baseline" "$pair"
run first-candidate "$candidate" "$pair"
run reverse-candidate "$candidate" "$reverse"
run reverse-baseline "$baseline" "$reverse"
for order in first reverse; do
  "$harness" compare "$out/$order-baseline/report.json" "$out/$order-candidate/report.json" \
    > "$out/$order.cross.json"
  # A historical compiler must actually fail identity, not a precondition
  # (missing file, invalid oracle, failing subprocess, etc.).
  if "$harness" zero-cost "$out/$order-baseline" > "$out/$order.red.json"; then
    echo "baseline unexpectedly passed" >&2
    exit 1
  fi
  jq -e '.passed == false and (.rows | length) == 3 and all(.rows[]; .equal == false)' \
    "$out/$order.red.json" > /dev/null
  "$harness" zero-cost "$out/$order-candidate" > "$out/$order.green.json"
done
sha256sum -c "$out/tools.sha256"
# Portable excerpt of the genuine reports. Full command journals, stage
# samples, diagnostics, ELF inputs and comparison output stay in FRESH_DIR.
jq -s --slurpfile first_red "$out/first.red.json" --slurpfile green "$out/first.green.json" \
  --slurpfile reverse_red "$out/reverse.red.json" --slurpfile reverse_green "$out/reverse.green.json" \
  '{schema:"on1x.zero-cost-evidence.v1", runs: [
     range(0;4) as $i | .[$i] | {
       order:["first-baseline","first-candidate","reverse-candidate","reverse-baseline"][$i],
       compiler_sha256:(.provenance.compiler_hash.stdout | split(" ")[0]),
       report_failures:.failures, rows:[.rows[] | {
         fixture,backend,valid,executable_bytes:.sizes_bytes.executable,
         compile_samples_ns,compile_stages,runtime_samples_ns}],
       mir:[.mir[] | {fixture,before:.metrics.before,after:.metrics.after}],
       identity:([$first_red[0],$green[0],$reverse_green[0],$reverse_red[0]][$i])
     }]}' "$out"/{first-baseline,first-candidate,reverse-candidate,reverse-baseline}/report.json \
  > "$out/evidence.json"
