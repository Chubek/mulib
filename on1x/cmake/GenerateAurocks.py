"""Compile lexical and contextual On1x fault banks with native Aurocks."""

from pathlib import Path
import re
import subprocess
import sys


def main(tool, source, output):
    text = source.read_text(encoding="utf-8")
    prefix, second, before_context, contextual, suffix = re.split(
        r"^/\* ON1X_(?:FAULT_BANK_(?:2|END)|CONTEXTUAL_FAULTS(?:_END)?) \*/$", text, flags=re.M)
    first_count = len(re.findall(r"^%fault ", prefix, re.M))
    second_count = len(re.findall(r"^%fault ", second, re.M))
    if not (0 < first_count <= 64 and 0 < second_count <= 64
            and 110 <= first_count + second_count <= 135):
        raise ValueError(f"expected 110–135 rules split into banks of at most 64, got {first_count}+{second_count}")
    if len(re.findall(r"^%fault ", contextual, re.M)) != 1:
        raise ValueError("expected exactly one contextual fault")

    # Aurocks validates the actual PEGs in each bank. Only the emitted tables
    # are needed from the supplemental banks; do not link additional parsers.
    # Use paths in the build tree so the authored grammar stays untouched.
    main_grammar = output / "on1x_scanner_primary.g"
    main_grammar.write_text(
        prefix + "\n" * sum(bank.count("\n") for bank in (second, before_context, contextual)) + suffix,
        encoding="utf-8")
    args = [tool, "--backend", "portable"]

    def write_bank(bank, start, lines_before, table_name, count):
        grammar = output / f"{start}.g"
        grammar.write_text(f"%start {start}\n" + "\n" * (lines_before - 1)
                           + bank + f"\n{start}: 'x';\n", encoding="utf-8")
        generated = subprocess.run([*args, str(grammar)], check=True, capture_output=True)
        table = re.search(rb"^static const aurocks_fault_rule aurocks_fault_rules\[\] = \{\n"
                          rb".*?^\};\n", generated.stdout, re.M | re.S)
        if not table or table[0].count(b"\n  {") != count:
            raise ValueError(f"Aurocks did not emit the expected {table_name} table")
        (output / f"{table_name}.h").write_bytes(
            table[0].replace(b"aurocks_fault_rules[]", table_name.encode() + b"[]", 1))

    write_bank(second, "on1x_scanner_secondary", prefix.count("\n"), "on1x_fault_bank_2", second_count)
    write_bank(contextual, "on1x_contextual_faults",
               sum(bank.count("\n") for bank in (prefix, second, before_context)),
               "on1x_contextual_fault_bank", 1)
    subprocess.run([*args, "-o", str(output / "on1x_parser.c"), str(main_grammar)], check=True)


if __name__ == "__main__":
    try:
        main(sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]))
    except subprocess.CalledProcessError as error:
        if error.stderr:
            sys.stderr.buffer.write(error.stderr)
        raise SystemExit(error.returncode)
    except ValueError as error:
        raise SystemExit(str(error))
