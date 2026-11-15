#!/bin/sh
# The French catalog must cover every string Raye adds, and the template must
# be the one the sources produce. Needs no locale: it reads the files.
set -eu
src="${RAYE_FILES_SOURCE_DIR:?}"
pot="$src/po/raye-files.pot"
po="$src/po/fr/raye-files.po"

for f in "$pot" "$po"; do
    [ -f "$f" ] || { echo "missing $f"; exit 1; }
done

if ! command -v msgcmp >/dev/null 2>&1; then
    # debian/rules sets RAYE_TESTS_STRICT=1 so that "a package build always
    # checks what it says it checks". This branch used to exit 0 under it
    # too, which would have made the whole catalog check a green no-op in a
    # chroot without gettext. gettext is an unconditional Build-Depends, so
    # it never fired; the promise is what keeps it from firing.
    if [ -n "${RAYE_TESTS_STRICT:-}" ]; then
        echo "gettext not installed (msgcmp)" >&2
        exit 1
    fi
    echo "gettext not installed, skipped"
    exit 0
fi

# Every msgid of the template is translated in French; a fuzzy entry counts
# as missing, which is what msgcmp does unless told otherwise.
if ! msgcmp "$po" "$pot"; then
    echo "the French catalog does not cover po/raye-files.pot"
    exit 1
fi

# The template is up to date with the sources.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
( cd "$src" && podir="$work" ./Messages.sh >/dev/null 2>&1 ) || {
    echo "Messages.sh failed"
    exit 1
}
# Compare the strings, not the date in the header nor the "#:" source lines:
# a line moving in an upstream file is not a new string.
grep -v -e '^"POT-Creation-Date:' -e '^#:' "$pot" > "$work/a"
grep -v -e '^"POT-Creation-Date:' -e '^#:' "$work/raye-files.pot" > "$work/b"
if ! diff -u "$work/a" "$work/b" > "$work/diff"; then
    echo "po/raye-files.pot is stale, run: cmake --build build --target raye-files-pot"
    cat "$work/diff"
    exit 1
fi
echo "ok"
