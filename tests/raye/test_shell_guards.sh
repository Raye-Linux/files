#!/bin/sh
# The guards of the other shell tests in this folder, checked by RUNNING
# them. Both silences below were measured on 2026-11-06, and neither showed
# in the package build log: ctest runs these with --output-on-failure, so a
# test that exits 0 having checked nothing prints nothing at all.
#
#   - test_desktop_file.sh printed "ok" and exited 0 when the .desktop file
#     was ABSENT. desktop-file-validate says "<path>: file does not exist"
#     and exits 1, but the script discarded the status (|| true) and then
#     looked only for ': error:' lines, which that message is not. A
#     renamed or moved desktop file would have gone green -- the same shape
#     as the renamed tests/ folder the ctest guard in debian/rules exists
#     for. Its two sibling scripts both carry a "[ -f ] || exit 1" guard;
#     this one did not, which is what made the omission visible.
#
#   - test_catalog.sh skipped with exit 0 when msgcmp was missing EVEN under
#     RAYE_TESTS_STRICT=1 -- the flag debian/rules sets so that "a package
#     build always checks what it says it checks". gettext is an
#     unconditional Build-Depends, so it has not fired yet; the promise is
#     what keeps it from firing the day that changes.
#
# Needs sh and coreutils only. Every case stops at the guard it is about,
# so the slow halves (Messages.sh, desktop-file-validate on a real file)
# run at most once.
set -eu
src="${RAYE_FILES_SOURCE_DIR:?}"
here="$src/tests/raye"

desktop_test="$here/test_desktop_file.sh"
catalog_test="$here/test_catalog.sh"
# The guard this file is about, applied to this file's own inputs.
for f in "$desktop_test" "$catalog_test"; do
    [ -f "$f" ] || { echo "missing $f"; exit 1; }
done

fail=0
n=0
bad() { echo "FAIL: $*"; fail=1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

expect() { # expect zero|nonzero DESCRIPTION -- command...
    want=$1
    desc=$2
    shift 2
    n=$((n + 1))
    got=0
    "$@" >"$work/out" 2>&1 || got=$?
    if [ "$want" = zero ]; then
        if [ "$got" -ne 0 ]; then
            bad "$desc: exit $got, wanted 0"
            sed 's/^/    /' "$work/out"
        fi
    else
        if [ "$got" -eq 0 ]; then
            bad "$desc: exit 0, wanted non-zero"
            sed 's/^/    /' "$work/out"
        fi
    fi
}

# A PATH with every command of the real PATH except one. Hiding a tool by
# name is the only way to exercise a "command -v" branch for real; a test
# that greps the script for the word RAYE_TESTS_STRICT would pass on a
# script that reads the variable and ignores it.
farm_without() {
    name=$1
    dir="$work/without-$name"
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir"
        oldifs=$IFS
        IFS=:
        for d in $PATH; do
            IFS=$oldifs
            [ -d "$d" ] || { IFS=:; continue; }
            for f in "$d"/*; do
                [ -e "$f" ] || continue
                b=${f##*/}
                [ "$b" = "$name" ] || [ -e "$dir/$b" ] || ln -s "$f" "$dir/$b" 2>/dev/null || true
            done
            IFS=:
        done
        IFS=$oldifs
        # The farm has to be usable, or every case below is a false verdict.
        [ -x "$dir/sh" ] || bad "farm_without $name: no sh in the farm"
        if PATH="$dir" command -v "$name" >/dev/null 2>&1; then
            bad "farm_without $name: $name is still reachable"
        fi
    fi
    echo "$dir"
}

# ---------------------------------------------------- test_desktop_file.sh
# A source tree that has a src/ but no desktop file in it.
mkdir -p "$work/nodesktop/src"
expect nonzero "test_desktop_file.sh on an ABSENT desktop file" \
    env RAYE_FILES_SOURCE_DIR="$work/nodesktop" sh "$desktop_test"

# ...and it must still pass on the real one, so the fix is a guard and not
# a blanket failure.
expect zero "test_desktop_file.sh on the real desktop file" \
    env -u RAYE_TESTS_STRICT RAYE_FILES_SOURCE_DIR="$src" sh "$desktop_test"

# Its missing-tool branch already honours RAYE_TESTS_STRICT; keep it that way.
nodfv=$(farm_without desktop-file-validate)
expect nonzero "test_desktop_file.sh, no desktop-file-validate, STRICT" \
    env PATH="$nodfv" RAYE_FILES_SOURCE_DIR="$src" RAYE_TESTS_STRICT=1 sh "$desktop_test"
expect zero "test_desktop_file.sh, no desktop-file-validate, not strict" \
    env -u RAYE_TESTS_STRICT PATH="$nodfv" RAYE_FILES_SOURCE_DIR="$src" sh "$desktop_test"

# -------------------------------------------------------- test_catalog.sh
nomsgcmp=$(farm_without msgcmp)
expect nonzero "test_catalog.sh, no msgcmp, STRICT" \
    env PATH="$nomsgcmp" RAYE_FILES_SOURCE_DIR="$src" RAYE_TESTS_STRICT=1 sh "$catalog_test"
expect zero "test_catalog.sh, no msgcmp, not strict (skip stays a skip)" \
    env -u RAYE_TESTS_STRICT PATH="$nomsgcmp" RAYE_FILES_SOURCE_DIR="$src" sh "$catalog_test"

# A missing catalog fails whether strict or not: it is not a missing tool.
mkdir -p "$work/nopo/po/fr"
expect nonzero "test_catalog.sh on an ABSENT catalog" \
    env -u RAYE_TESTS_STRICT RAYE_FILES_SOURCE_DIR="$work/nopo" sh "$catalog_test"

[ "$fail" = 0 ] || exit 1
echo "ok: $n checks, 0 failed"
