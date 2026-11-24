"""Exercise the generated fault runtime through the actual On1x command."""
import json
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory


compiler = Path(sys.argv[1]).resolve()

with TemporaryDirectory(prefix="on1x-faults-") as work:
    source = Path(work) / "sample.on1x"

    def run(data, mode="--syntax-faults"):
        source.write_bytes(data)
        result = subprocess.run([compiler, mode, source], capture_output=True, timeout=20)
        assert source.read_bytes() == data, "fault inspection changed the original file"
        return result

    if "--bootstrap" in sys.argv:
        result = run(b"fn main(){}")
        assert result.returncode == 2 and b"requires the generated Aurocks scanner" in result.stderr
        assert result.stdout == b""
        raise SystemExit(0)

    valid = 'fn main(){ println("雪 ≤"); } # NBSP \u00a0\n'.encode()
    result = run(valid)
    assert result.returncode == 0 and result.stdout == valid and not result.stderr, result

    bom = b"\xef\xbb\xbf"
    result = run(bom + b"fn main(){}")
    assert result.returncode == 0 and result.stdout == b"fn main(){}" and not result.stderr, result
    result = run(b"fn" + bom + b"main(){}")
    assert result.returncode == 0 and result.stdout == b"fn main(){}" and not result.stderr, result
    result = run(bom + "fn main（）{ println(42)； }".encode())
    assert result.returncode == 0 and result.stdout == b"fn main(){ println(42); }", result
    assert result.stderr.count(b"aurocks: patch at byte") == 3, result
    assert b"replace fullwidth semicolon" in result.stderr, result

    # Bank one wins for specific rules. Bank two still applies after a patch.
    rejected = [
        ("let x=1≤2;".encode(), b"info at byte 7: use <="),
        ("let x=a≡b;".encode(), b"warning at byte 7: On1x has no identity glyph"),
        (br'let x=b!"\xZ0";', b"error at byte 6: invalid or unterminated byte string"),
        (b"let x=r!'bad", b"error at byte 6: single apostrophe"),
        (b"let x=```unterminated", b"error at byte 6: unterminated triple-backtick"),
        ("let x=“string”;".encode(), b"info at byte 6: use straight ASCII double quotes"),
        ("let\u00a0x=1;".encode(), b"warning at byte 3: non-breaking space"),
        ("fn main()｛-".encode(), b"warning at byte 9: fullwidth opening brace was not repaired"),
        ("fn main()｛@".encode(), b"warning at byte 9: fullwidth opening brace was not repaired"),
        ("let x=é;".encode(), b"error at byte 6: unsupported Unicode character"),
        (b"let x=\x80;", b"error at byte 6: invalid leading byte"),
        (b"let x=\xed\xa0\x80;", b"error at byte 6: invalid token or malformed UTF-8"),
        (b"let x=\xf0\x9f", b"error at byte 6: invalid token or malformed UTF-8"),
        ("let x=1；\u200b".encode(), b"warning at byte 8: zero-width space"),
    ]
    for data, message in rejected:
        result = run(data)
        assert result.returncode == 1 and not result.stdout and message in result.stderr, (data, result)

    # Ordinary compilation must reject, not apply, any proposal.
    result = run(bom + b"fn main(){}", "--check")
    assert result.returncode == 1 and not result.stdout and b"invalid token" in result.stderr, result
    assert b"aurocks:" not in result.stderr, result

    result = run(bom * 9 + b"fn main(){}")
    assert result.returncode == 1 and not result.stdout and b"eight edits" in result.stderr, result
    result = run(b"fn main(){}\0")
    assert result.returncode == 1 and not result.stdout and b"NUL in source" in result.stderr, result
    result = run(b"#" * (1 << 20))
    assert result.returncode == 2 and not result.stdout and b"smaller than 1 MiB" in result.stderr, result

    # One independent witness for every declaration, in source order.
    # Decode only the directive's outer strings to compare the exact action
    # and message. The PEGs themselves are matched only by Aurocks.
    witnesses = [
        # File signatures and unsupported ASCII tokens.
        bom, b"\xff\xfe", b"\xfe\xff", b"\x0e\xfe\xff", b"\xfb\xee\x28",
        b"\x84\x31\x95\x33", b"\xdd\x73\x66\x73", b"\xf7\x64\x4c",
        b"\\\n", b"\\name", b"\\", b"&=", b"&", b"^=", b"^",
        # Operator glyphs and exact fullwidth delimiter repairs.
        *"≔≕≡≈≢∈∉∧∨¬∷∅∞×÷→←⇒≤≥≠−±…",
        *"；，（）［］｛｝",
        # Broken comment, character, command, and string openers.
        "{-", "{@", "{!", "{>", "b!#\\", "#\\", "```", "`",
        "'", 'r!"', 'b!"', '"""', '"',
        # Typography, including the brace that must not create a comment.
        "‘", "“", "«", "‹", "′", "＂", "｀", "「", "｛-", "：", "．",
        "！", "？", "％", "＋", "－", "＊", "／", "＝", "＜", "｜", "＃",
        "＠", "＄", "～", "＿", "、", "。", "·", "•", "–", "∕", "∗", "⋅",
        "〈", "【", ";", "„", "ʼ",
        # Invisible controls, invalid encodings, and final fallback.
        "\u00a0", "\u2002", "\u200b", "\u200c", "\u2060", "\u00ad", "\u180e",
        "\u200e", "\u202e", "\u2066", "\u2028", "\u0085", "\v", "\f",
        b"\x1b", b"\x7f", b"\x01", "\u0081", b"\x80", "é", b"\xf0\x9f",
    ]
    grammar = (Path(__file__).resolve().parents[1] / "grammar/On1x.g").read_text(encoding="utf-8")
    directives = [line for line in grammar.splitlines() if line.startswith("%fault ")]
    assert 110 <= len(directives) == len(witnesses) <= 135
    quoted = r'"(?:\\.|[^"\\])*"'
    for directive, sample in zip(directives, witnesses):
        data = sample.encode() if isinstance(sample, str) else sample
        rule = re.fullmatch(rf"%fault {quoted} => (\w+) ({quoted})(?: ({quoted}))?"
                            r" using on1x_fault_hint", directive)
        assert rule, directive
        action, payload, message = rule.groups()
        result = run(data)
        if action.endswith("_patch"):
            assert result.returncode == 0 and result.stdout == json.loads(payload).encode(), (directive, result)
            if action == "silent_patch":
                assert not result.stderr, (directive, result)
            else:
                assert f"patch at byte 0: {json.loads(message)}\n".encode() in result.stderr, (directive, result)
        else:
            label = {"inform": "info", "warn": "warning", "error": "error"}[action]
            assert result.returncode == 1 and not result.stdout, (directive, result)
            assert f"{label} at byte 0: {json.loads(payload)}\n".encode() in result.stderr, (directive, result)
