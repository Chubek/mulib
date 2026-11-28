#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
    printf 'Usage: sh tests/run.sh /path/to/vim-or-nvim\n' >&2
    exit 2
fi
editor=$(command -v "$1") || { printf 'Editor not found: %s\n' "$1" >&2; exit 1; }
case $editor in /*) ;; *) editor=$PWD/$editor ;; esac
here=$(CDPATH= cd -P "$(dirname "$0")" && pwd -P)
version=$("$editor" --version)
case $version in
    NVIM*) mode=--nvim ;;
    'VIM - Vi IMproved'*) mode=--vim ;;
    *) printf 'Expected Vim or Neovim: %s\n' "$editor" >&2; exit 1 ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/on1x-vim.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
unset VIM VIMRUNTIME
export HOME="$work/home with spaces"
export XDG_CONFIG_HOME="$work/xdg"
export XDG_DATA_HOME="$work/data"
export XDG_STATE_HOME="$work/state"
export XDG_CACHE_HOME="$work/cache"
export NVIM_APPNAME=on1x-test
mkdir -p "$HOME"
cd "$work"

case $mode in
    --nvim) export ON1X_RUNTIME="$XDG_CONFIG_HOME/$NVIM_APPNAME/pack/on1x/start/on1x" ;;
    --vim) export ON1X_RUNTIME="$HOME/.vim/pack/on1x/start/on1x" ;;
esac
sh "$here/../install.sh" "$mode"
sh "$here/../install.sh" "$mode" 'custom runtime | data'
printf 'unrelated\n' > "$ON1X_RUNTIME/doc/unrelated"
printf 'old\n' > "$ON1X_RUNTIME/ftdetect/on1x.vim"
sh "$here/../install.sh" "$mode"
test "$(cat "$ON1X_RUNTIME/doc/unrelated")" = unrelated
test ! -e "$ON1X_RUNTIME/tests"
cmp 'custom runtime | data/doc/on1x.txt' "$ON1X_RUNTIME/doc/on1x.txt"
if sh "$here/../install.sh" "$mode" --bad >/dev/null 2>&1; then
    printf 'Installer accepted an unknown option\n' >&2
    exit 1
fi
mkdir -p blocked/syntax/on1x.vim
if sh "$here/../install.sh" "$mode" blocked >/dev/null 2>&1; then
    printf 'Installer accepted a directory in place of a file\n' >&2
    exit 1
fi
test ! -e blocked/ftdetect/on1x.vim

# Generate tags using the actual editor passed to the test, even if it is
# installed under a nonstandard name or outside PATH.
export ON1X_DOC_DIR="$ON1X_RUNTIME/doc"
"$editor" -u NONE -i NONE -n -es \
    -c 'try | execute "helptags " . fnameescape($ON1X_DOC_DIR) | catch | cquit | endtry' \
    -c 'qa!'
printf 'set nocompatible\nsyntax enable\nfiletype plugin indent on\nset formatoptions+=t\n' > vimrc
export ON1X_TEST_LOG="$work/results"
if ! "$editor" -u "$work/vimrc" -i NONE -n -es -V1"$work/editor.log" -S "$here/editing.vim"; then
    if [ -f "$ON1X_TEST_LOG" ]; then
        cat "$ON1X_TEST_LOG" >&2
    fi
    cat "$work/editor.log" >&2
    exit 1
fi
