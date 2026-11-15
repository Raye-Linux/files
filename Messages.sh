#! /usr/bin/env bash
# Builds po/raye-files.pot, the catalog of every string Raye adds to Files.
#
# - src/raye/** is Raye code: all the usual i18n calls.
# - Upstream files (the rest of src/) keep Dolphin's own catalog. A string
#   Raye adds there is written i18nd("raye-files", ...) (or i18ndc, i18ndp,
#   i18ndcp), and only those calls are taken from them.
#
# Run from anywhere: ./Messages.sh. When the translation scripts call it,
# they set XGETTEXT, EXTRACTRC and podir themselves.
set -eu
cd "$(dirname "$0")"

podir=${podir:-$PWD/po}
if [ -z "${XGETTEXT:-}" ]; then
    XGETTEXT="xgettext --from-code=UTF-8 -C --kde -ci18n \
        -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 \
        -ki18nd:2 -ki18ndc:2c,3 -ki18ndp:2,3 -ki18ndcp:2c,3,4 \
        -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
        -kxi18n:1 -kxi18nc:1c,2 -kxi18np:1,2 -kxi18ncp:1c,2,3 \
        -kkxi18n:1 -kkxi18nc:1c,2 -kkxi18np:1,2 -kkxi18ncp:1c,2,3 \
        -kI18N_NOOP:1 -kI18NC_NOOP:1c,2 \
        -kqsTr:1 -kqsTranslate:2 -kqsTrId:1 \
        --copyright-holder=techoraye --package-name=raye-files \
        --msgid-bugs-address=https://github.com/techoraye"
fi
EXTRACTRC=${EXTRACTRC:-extractrc}

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Raye code.
find src/raye -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.qml' -o -name '*.js' \) 2>/dev/null \
    | sort > "$work/raye.list"
rc=$(find src/raye -type f \( -name '*.ui' -o -name '*.rc' -o -name '*.kcfg' \) 2>/dev/null | sort)
if [ -n "$rc" ]; then
    # Without extractrc the strings of these files would be dropped quietly.
    if ! command -v "$EXTRACTRC" >/dev/null 2>&1; then
        echo "$EXTRACTRC not found: needed for the .ui, .rc and .kcfg files of src/raye" >&2
        exit 1
    fi
    # shellcheck disable=SC2086
    $EXTRACTRC $rc > "$work/rc.cpp"
    echo "$work/rc.cpp" >> "$work/raye.list"
fi

# Upstream files that carry Raye strings. Only the i18nd forms are taken:
# upstream already calls xi18ndc and friends with Dolphin's own domain, and
# xgettext cannot tell the two domains apart in one file. So a ki18nd,
# xi18nd or kxi18nd call with the Raye domain is refused, not dropped.
if grep -rnE '(k|x|kx)i18nd(c|p|cp)?\([[:space:]]*"raye-files"' src --include='*.cpp' --include='*.h' 2>/dev/null \
        | grep -v '^src/raye/'; then
    echo "write Raye strings in upstream files with i18nd(\"raye-files\", ...)" >&2
    exit 1
fi
grep -rlE '(^|[^kx])i18nd(c|p|cp)?\([[:space:]]*"raye-files"' src --include='*.cpp' --include='*.h' 2>/dev/null \
    | grep -v '^src/raye/' | sort > "$work/upstream.list" || true

: > "$work/raye.pot"
if [ -s "$work/raye.list" ]; then
    # shellcheck disable=SC2046,SC2086
    $XGETTEXT -f "$work/raye.list" -o "$work/raye.pot"
fi
: > "$work/upstream.pot"
if [ -s "$work/upstream.list" ]; then
    # Only the domain calls; the plain i18n() of upstream files is Dolphin's.
    # shellcheck disable=SC2046
    xgettext --from-code=UTF-8 -C -ci18n -k \
        -ki18nd:2 -ki18ndc:2c,3 -ki18ndp:2,3 -ki18ndcp:2c,3,4 \
        --copyright-holder=techoraye --package-name=raye-files \
        --msgid-bugs-address=https://github.com/techoraye \
        -f "$work/upstream.list" -o "$work/upstream.pot"
fi

set --
[ -s "$work/raye.pot" ] && set -- "$@" "$work/raye.pot"
[ -s "$work/upstream.pot" ] && set -- "$@" "$work/upstream.pot"
mkdir -p "$podir"
if [ "$#" -eq 0 ]; then
    echo "no Raye strings found" >&2
    exit 1
fi
msgcat --use-first --sort-by-file "$@" -o "$work/merged.pot"

# A readable header instead of gettext's placeholders, and no "fuzzy" flag
# on it: msgmerge refuses to use a fuzzy template header.
sed -e '1s|.*|# Raye Files: the strings Raye adds to the file explorer.|' \
    -e '2s|.*|# Copyright (C) 2026 techoraye|' \
    -e '/^# FIRST AUTHOR/d' \
    -e '/^#, fuzzy$/d' \
    -e 's|^"Content-Type: text/plain; charset=CHARSET|"Content-Type: text/plain; charset=UTF-8|' \
    -e '/^"PO-Revision-Date: YEAR/d' \
    -e '/^"Last-Translator: FULL NAME/d' \
    -e 's|^"Language-Team: LANGUAGE <LL@li.org>|"Language-Team: Raye|' \
    -e '/^"Language: \\n"/d' \
    "$work/merged.pot" > "$podir/raye-files.pot"
echo "wrote $podir/raye-files.pot"
