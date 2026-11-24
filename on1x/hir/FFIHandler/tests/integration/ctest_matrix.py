#!/usr/bin/env python3
"""Record independently selected CTest groups and fail on empty required groups."""

import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import xml.etree.ElementTree as ET


GROUPS = {
    "ffi": r"^on1x\.ffihandler[._]",
    "kernel": r"^on1x\..*(kernel|mulib)",
    "contract": r"^on1x\..*contract",
    "abi": r"^on1x\..*abi",
    "c11": r"^on1x\..*c11",
    "hir": r"^on1x\..*(ownership|hir|memgraph|processor)",
    "project": r"^on1x\..*project",
    "install": r"^on1x\..*install",
    "native": r"^on1x\.(cephyr\.|stdlib\.cephyr)",
    "trust-dso": r"^on1x\.(kernel_review_sdk|kernel_dynlib)$",
    "full": None,
}


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("build", type=Path)
    cli.add_argument("logs", type=Path, help="new directory; never overwrites a run")
    cli.add_argument("--group", action="append", choices=GROUPS)
    cli.add_argument("--absent", action="append", default=[], metavar="GROUP=REASON",
                     help="explicitly document an unavailable group; fails if it is present")
    cli.add_argument("--list-only", action="store_true")
    cli.add_argument("--jobs", type=int, default=2)
    a = cli.parse_args()
    if a.jobs < 1:
        cli.error("--jobs must be positive")
    groups = list(dict.fromkeys(a.group or GROUPS))
    absent = {}
    for option in a.absent:
        group, sep, reason = option.partition("=")
        if not sep or not reason or group not in groups or group == "full" or group in absent:
            cli.error(f"invalid absence: {option!r}")
        absent[group] = reason
    build = a.build.resolve()
    logs = a.logs.resolve()
    logs.mkdir(parents=True)
    tmp = logs / "tmp"
    tmp.mkdir()
    env = {**os.environ, "TMPDIR": str(tmp), "LC_ALL": "C"}
    base = ["ctest", "--test-dir", str(build)]

    def run(name, argv):
        with (logs / "commands.log").open("a") as commands:
            print(shlex.join(argv), file=commands)
        with (logs / name).open("w") as out:
            return subprocess.run(argv, stdout=out, stderr=subprocess.STDOUT, env=env).returncode

    if run("ctest-version.log", ["ctest", "--version"]):
        raise SystemExit("could not read CTest version")
    # Preserve relevant selections whether supplied through flags or CMake defaults.
    cache = build / "CMakeCache.txt"
    if cache.is_file():
        keys = re.compile(r"^(?:ON1X_|BUILD_TESTING:|CMAKE_(?:BUILD_TYPE|"
                          r"[A-Z]+_COMPILER|[A-Z]+_FLAGS|GENERATOR|INSTALL_PREFIX)|Cephyr_DIR:)")
        (logs / "cmake-options.log").write_text(
            "\n".join(line for line in cache.read_text().splitlines() if keys.match(line)) + "\n")

    report = {}
    for group in groups:
        select = ["-R", GROUPS[group]] if GROUPS[group] else []
        listing = f"{group}-N.log"
        rc = run(listing, [*base, "-N", *select])
        count = re.search(r"^Total Tests: (\d+)$", (logs / listing).read_text(), re.M)
        n = int(count.group(1)) if count else None
        row = report[group] = dict(selected=n, passed=0, failed=0, skipped=0,
                                   not_run=0, list_status=rc)
        if rc or n is None:
            row["result"] = "discovery-error"
        elif group in absent:
            row.update(result="unavailable" if n == 0 else "unexpected-tests",
                       reason=absent[group])
        elif n == 0:
            row["result"] = "empty-required-group"
        elif a.list_only:
            row.update(result="listed-only", not_run=n)
        else:
            xml = logs / f"{group}.xml"
            rc = run(f"{group}.log", [*base, *select, "--no-tests=error", "--output-on-failure",
                                      "--parallel", str(a.jobs), "--output-junit", str(xml)])
            row["run_status"] = rc
            try:
                cases = ET.parse(xml).getroot().findall(".//testcase")
            except (OSError, ET.ParseError):
                row.update(result="missing-junit", not_run=n)
                continue
            excluded = {}
            for case in cases:
                status = case.get("status")
                skip = case.find("skipped")
                reason = skip.get("message", "") if skip is not None else ""
                if reason in ("Unable to find executable", "Disabled") or status == "disabled":
                    row["not_run"] += 1
                    excluded[case.get("name")] = reason or status
                elif case.find("failure") is not None or case.find("error") is not None:
                    row["failed"] += 1
                elif skip is not None:
                    row["skipped"] += 1
                    excluded[case.get("name")] = reason or status
                elif status == "run":
                    row["passed"] += 1
                else:
                    row["not_run"] += 1
                    excluded[case.get("name")] = status
            if excluded:
                row["excluded_cases"] = excluded
            row["not_run"] += max(0, n - len(cases))
            row["result"] = (
                "failed" if rc or row["failed"] or row["not_run"] or len(cases) != n
                else "passed-with-skips" if row["skipped"] and row["passed"]
                else "all-skipped" if row["skipped"]
                else "passed")
    print(json.dumps(report, indent=2))
    (logs / "summary.json").write_text(json.dumps(report, indent=2) + "\n")
    good = {"passed", "passed-with-skips", "unavailable", "listed-only"}
    return int(any(row["result"] not in good for row in report.values()))


if __name__ == "__main__":
    raise SystemExit(main())
