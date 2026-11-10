#!/bin/sh
set -eu
parallel=$1
shebang=$2
work=$3
expect_status() {
  expected=$1; shift
  status=0
  "$@" > "$work/status.out" 2> "$work/status.err" || status=$?
  if [ "$status" -ne "$expected" ]; then
    echo "Expected status $expected, got $status: $*" >&2
    cat "$work/status.err" >&2
    exit 1
  fi
}
printf 'a\000\000b c\000' > "$work/inputs"
"$parallel" -k -0 -a "$work/inputs" "printf '<%s>\\n' {}" > "$work/out"
printf '<a>\n<>\n<b c>\n' > "$work/expected"
cmp "$work/out" "$work/expected"
printf 'one\r\n\ntwo' > "$work/inputs"
"$parallel" -k "printf '%s\\n' {}" :::: "$work/inputs" > "$work/out"
printf 'one\ntwo\n' > "$work/expected"
cmp "$work/out" "$work/expected"
"$parallel" --dry-run 'echo {/.}' ::: /dir/file.txt > "$work/out"
printf "echo 'file'\n" > "$work/expected"
cmp "$work/out" "$work/expected"
expect_status 2 "$parallel" --jobs=-1 echo ::: x
expect_status 2 "$parallel" --jobs ' 2' echo ::: x
expect_status 2 "$parallel" --jobs +2 echo ::: x
expect_status 2 "$parallel" --halt now,fail=1 echo ::: x
expect_status 2 "$parallel" --lua-code 'task("true")' echo ::: x
expect_status 2 "$parallel" -a "$work/inputs" echo ::: x
expect_status 2 "$parallel"
expect_status 1 "$parallel" --shell /nonexistent/shell echo ::: x
expect_status 1 "$parallel" --lua "$work/missing.lua"
printf 'a\000b\n' > "$work/binary"
expect_status 1 "$parallel" -a "$work/binary" echo

printf '${WORD}\n${1}\n' > "$work/file with spaces"
"$shebang" "$work/file with spaces" 'a b' > "$work/out"
printf '${WORD}\na b\n' > "$work/expected"
cmp "$work/out" "$work/expected"
"$shebang" '-DWORD=hello world' "$work/file with spaces" ok > "$work/out"
printf 'hello world\nok\n' > "$work/expected"
cmp "$work/out" "$work/expected"
"$shebang" --define='WORD=hello world' "$work/file with spaces" ok > "$work/out"
cmp "$work/out" "$work/expected"
"$shebang" --shebang-args "-D WORD='hello world' -F upper" "$work/file with spaces" ok > "$work/out"
printf 'HELLO WORLD\nOK\n' > "$work/expected"
cmp "$work/out" "$work/expected"
printf 'hello\n' | "$shebang" --filter upper - > "$work/out"
printf 'HELLO\n' > "$work/expected"
cmp "$work/out" "$work/expected"
printf 'abc\n' > "$work/short"
expect_status 0 "$shebang" --max-line=3 --max-expansion 3 "$work/short"
expect_status 1 "$shebang" --max-line=2 "$work/short"
expect_status 1 "$shebang" --max-expansion=2 "$work/short"
expect_status 1 "$shebang" --strict-macros "$work/file with spaces"
expect_status 1 "$shebang" "$work/binary"
expect_status 2 "$shebang" --max-line=-1 "$work/short"
expect_status 2 "$shebang" --max-expansion 9999999999999999999999999999 "$work/short"
expect_status 2 "$shebang" --shebang-args "-D X='unclosed" "$work/short"
expect_status 127 "$shebang" --exec=/nonexistent/program "$work/short"
printf 'printf "%%s\\n" "$1"\nexit 13\n' > "$work/execute"
expect_status 13 "$shebang" --exec=/bin/sh "$work/execute" 'argument with spaces'
printf 'argument with spaces\n' > "$work/expected"
cmp "$work/status.out" "$work/expected"

# Invoke the interpreter through an actual kernel shebang with a bundled argv[1].
cat > "$work/kernel-script" << SCRIPT
#!$shebang -D WORD='hello world' -F upper
\${WORD} \${1}
SCRIPT
chmod +x "$work/kernel-script"
"$work/kernel-script" friend > "$work/out"
printf 'HELLO WORLD FRIEND\n' > "$work/expected"
cmp "$work/out" "$work/expected"
