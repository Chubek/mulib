"""Compile the two banks of On1x runtime faults with native Aurocks."""

from pathlib import Path
import re
import subprocess
import sys


def main(tool, source, output):
    text = source.read_text(encoding="utf-8")
    prefix, second, suffix = re.split(r"^/\* ON1X_FAULT_BANK_(?:2|END) \*/$", text, flags=re.M)
    first_count = len(re.findall(r"^%fault ", prefix, re.M))
    second_count = len(re.findall(r"^%fault ", second, re.M))
    if not (0 < first_count <= 64 and 0 < second_count <= 64
            and 110 <= first_count + second_count <= 135):
        raise ValueError(f"expected 110–135 rules split into banks of at most 64, got {first_count}+{second_count}")

    # Aurocks validates the actual PEGs in both banks. Only the emitted table
    # is needed from the second translation unit; do not link a second parser.
    # Use paths in the build tree so the authored grammar stays untouched.
    main_grammar = output / "on1x_scanner_primary.g"
    second_grammar = output / "on1x_scanner_secondary.g"
    main_grammar.write_text(prefix + "\n" * second.count("\n") + suffix, encoding="utf-8")
    second_grammar.write_text(
        "%start on1x_secondary_faults\n" + "\n" * (prefix.count("\n") - 1)
        + second + "\non1x_secondary_faults: 'x';\n",
        encoding="utf-8",
    )
    args = [tool, "--backend", "portable"]
    secondary = subprocess.run([*args, str(second_grammar)], check=True, capture_output=True)
    table = re.search(rb"^static const aurocks_fault_rule aurocks_fault_rules\[\] = \{\n"
                      rb".*?^\};\n", secondary.stdout, re.M | re.S)
    if not table or table[0].count(b"\n  {") != second_count:
        raise ValueError("Aurocks did not emit the expected secondary fault table")
    subprocess.run([*args, "-o", str(output / "on1x_parser.c"), str(main_grammar)], check=True)
    (output / "on1x_fault_bank_2.h").write_bytes(
        table[0].replace(b"aurocks_fault_rules[]", b"on1x_fault_bank_2[]", 1)
    )


if __name__ == "__main__":
    try:
        main(sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]))
    except subprocess.CalledProcessError as error:
        if error.stderr:
            sys.stderr.buffer.write(error.stderr)
        raise SystemExit(error.returncode)
    except ValueError as error:
        raise SystemExit(str(error))
