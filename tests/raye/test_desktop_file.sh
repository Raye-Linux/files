#!/bin/sh
# desktop-file-validate on the Files desktop file. InitialPreference is an
# old KDE key the validator does not know; KService still reads it to rank
# Files first for folders, so that one error is allowed.
set -eu
file="${RAYE_FILES_SOURCE_DIR:?}/src/org.kde.dolphin.desktop"

# A file that is not there is a failure, not a pass -- the guard both
# sibling scripts in this folder already carry. Without it,
# desktop-file-validate's "<path>: file does not exist" slipped past the
# ': error:' filter below and this script printed "ok" and exited 0, so a
# renamed or moved desktop file went green. Measured 2026-11-06.
[ -f "$file" ] || { echo "missing $file"; exit 1; }

if ! command -v desktop-file-validate >/dev/null 2>&1; then
    if [ -n "${RAYE_TESTS_STRICT:-}" ]; then
        echo "desktop-file-validate not installed" >&2
        exit 1
    fi
    echo "desktop-file-validate not installed, skipped"
    exit 0
fi

status=0
out=$(desktop-file-validate --no-hints "$file" 2>&1) || status=$?
errors=$(printf '%s\n' "$out" | grep ': error:' || true)
unexpected=$(printf '%s\n' "$errors" | grep -v '"InitialPreference"' || true)
if [ -n "$unexpected" ]; then
    printf '%s\n' "$unexpected"
    exit 1
fi
# The status is kept, but it cannot be tested on its own: the allowed
# InitialPreference error makes the validator exit 1 on a correct file. A
# non-zero status with no ': error:' line to account for it is the
# validator refusing the file for another reason -- unreadable, or not a
# desktop entry at all -- which used to be discarded by "|| true".
if [ "$status" -ne 0 ] && [ -z "$errors" ]; then
    printf '%s\n' "$out"
    echo "desktop-file-validate exited $status without an error line"
    exit 1
fi
echo "ok: 2 checks, 0 failed"
