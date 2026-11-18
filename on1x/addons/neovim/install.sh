#!/usr/bin/env sh

set -eu

usage() {
    cat <<'EOF'
Usage: sh install.sh [--nvim|--vim] [DEST]
Install the On1x editor runtime as a per-user start package.

  --nvim    use ${XDG_CONFIG_HOME:-$HOME/.config}/nvim/pack/on1x/start/on1x
            (respects NVIM_APPNAME; default)
  --vim     use $HOME/.vim/pack/on1x/start/on1x
  DEST      install directly into this runtime directory instead
  --help    show this message

No compiler or LSP server is copied or run. Existing On1x runtime files
are updated; unrelated files in the destination are not removed.
EOF
}

die() {
    printf 'On1x: %s\n' "$*" >&2
    exit 1
}

editor=nvim
case ${1:-} in
    --nvim) shift ;;
    --vim) editor=vim; shift ;;
esac
case ${1:-} in
    -h|--help) usage; exit 0 ;;
    --) shift ;;
    -*) usage >&2; exit 2 ;;
esac
[ "$#" -le 1 ] || { usage >&2; exit 2; }
if [ "$#" -eq 1 ]; then
    [ -n "$1" ] || die 'destination must not be empty'
    dest=$1
elif [ "$editor" = vim ]; then
    dest=${HOME:?Set HOME or specify a destination}/.vim/pack/on1x/start/on1x
else
    dest=${XDG_CONFIG_HOME:-${HOME:?Set HOME or specify a destination}/.config}
    dest=$dest/${NVIM_APPNAME:-nvim}/pack/on1x/start/on1x
fi
case $dest in
    /*) ;;
    *) dest=$PWD/$dest ;;
esac
src=$(CDPATH= cd -P "$(dirname "$0")" && pwd -P)

# Deliberate manifest: do not overwrite unrelated editor config or copy tests.
files='ftdetect/on1x.vim ftplugin/on1x.vim indent/on1x.vim syntax/on1x.vim doc/on1x.txt lua/on1x.lua'
for file in $files; do
    dir=$dest/${file%/*}
    [ -f "$src/$file" ] && [ -r "$src/$file" ] || die "missing source file: $src/$file"
    if [ -e "$dir" ] && [ ! -d "$dir" ]; then
        die "cannot create a directory at $dir"
    fi
    [ ! -d "$dest/$file" ] || die "destination file is a directory: $dest/$file"
done

tmp=
trap '[ -z "$tmp" ] || rm -f "$tmp"' EXIT
trap 'exit 1' HUP INT TERM
for file in $files; do
    dir=$dest/${file%/*}
    mkdir -p "$dir"
    tmp=$(mktemp "$dir/.on1x.XXXXXXXX")
    cat "$src/$file" > "$tmp"
    chmod 644 "$tmp"
    mv -f "$tmp" "$dest/$file"
    tmp=
done

if command -v "$editor" >/dev/null 2>&1; then
    # Use environment data, not interpolation into an Ex command.
    ON1X_DOC_DIR=$dest/doc "$editor" -u NONE -i NONE -n -es \
        -c 'try | execute "helptags " . fnameescape($ON1X_DOC_DIR) | catch | cquit | endtry' \
        -c 'qa!' || die "runtime copied but helptags failed; run :helptags for $dest/doc"
else
    printf 'On1x: %s unavailable; run :helptags for %s/doc in your editor.\n' "$editor" "$dest" >&2
fi
printf 'On1x runtime installed to %s\n' "$dest"
