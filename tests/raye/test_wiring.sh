#!/bin/sh
# What decides whether the tests of this folder run at all. Nothing here
# looks at a build tree: these are the three couplings that, if they drift,
# stop tests from running without a word.
#
# Why. On 2026-11-06 the theme repository was found with a debian/rules
# that ENUMERATED test folders and had missed two: 346 tests the package
# build never ran, and no sign of it anywhere. This repository is wired the
# other way round -- tests/raye/CMakeLists.txt GLOBS every test_*.cpp,
# test_*.sh and test_*.py and labels them "raye", and debian/rules runs
# "ctest -L raye" -- so no file can fall outside a hand-written list. That
# safety holds only while all three parts agree, and each can be broken on
# its own:
#
#   1. file(GLOB) is NOT recursive. A test_*.cpp in a SUBFOLDER of this
#      directory is never registered, never compiled, and never missed.
#      This folder already has subfolders (harness/, data/).
#   2. the globs match the three extensions and nothing else. This check
#      earned its keep on 2026-11-06: a test_*.py was added here while
#      the CMakeLists globbed .cpp and .sh only, and this script is what
#      said so -- the glob and this list were then both extended, in that
#      order. Anything else dropped in here is still picked up by nothing.
#   3. the label is the whole coupling: LABELS raye in the CMakeLists on
#      one side, -L raye in debian/rules on the other. Drop LABELS from one
#      of the three loops and those tests leave the package build silently,
#      while ctest still prints "100% tests passed".
#
# Needs sh and find only.
set -eu
src="${RAYE_FILES_SOURCE_DIR:?}"
here="$src/tests/raye"
lists="$here/CMakeLists.txt"
rules="$src/debian/rules"

for f in "$lists" "$rules"; do
    [ -f "$f" ] || { echo "missing $f"; exit 1; }
done

fail=0
n=0
bad() { echo "FAIL: $*"; fail=1; }
checked() { n=$((n + 1)); }

# ---- 1. nothing hides in a subfolder, where the glob would not see it
checked
hidden=$(find "$here" -mindepth 2 \( -name 'test_*.cpp' -o -name 'test_*.sh' -o -name 'test_*.py' \) -print)
if [ -n "$hidden" ]; then
    bad "these files are in a subfolder of tests/raye, which file(GLOB) does not read, so nothing runs them:"
    printf '%s\n' "$hidden" | sed 's/^/    /'
fi

# ---- 2. every test file here has an extension one of the two globs matches
checked
for f in "$here"/test_*; do
    [ -e "$f" ] || continue
    case ${f##*/} in
        *.cpp|*.sh|*.py) ;;
        *) bad "${f##*/}: tests/raye/CMakeLists.txt globs test_*.cpp, test_*.sh and test_*.py only, so this file is registered nowhere" ;;
    esac
done

# ---- 3. the CMakeLists still globs, and still labels, both kinds
checked
for pat in 'test_\*\.cpp' 'test_\*\.sh' 'test_\*\.py'; do
    grep -q "file(GLOB.*$pat" "$lists" \
        || bad "tests/raye/CMakeLists.txt no longer globs $pat: a hand-written list is what hid 346 tests in theme"
done

checked
labels=$(grep -c 'LABELS raye' "$lists")
if [ "$labels" -lt 3 ]; then
    bad "tests/raye/CMakeLists.txt sets LABELS raye $labels time(s); all three loops (test_*.cpp, test_*.sh and test_*.py) need it, or debian/rules' -L raye drops the unlabelled ones"
fi

# ---- 4. the other end of that coupling
checked
grep -q 'ARGS+="-L raye' "$rules" \
    || bad "debian/rules no longer runs ctest with -L raye: either the label is gone from the build, or it now filters on something this folder does not set"

checked
grep -q 'CTEST_NO_TESTS_ACTION[[:space:]]*=[[:space:]]*error' "$rules" \
    || bad "debian/rules does not export CTEST_NO_TESTS_ACTION=error: with a label that matches nothing, ctest exits 0 (measured 2026-11-06: exit 0 without it, exit 8 with it)"

# ---- 5. the count the package build should run, stated out loud
checked
cpp=$(find "$here" -maxdepth 1 -name 'test_*.cpp' | wc -l)
sh_=$(find "$here" -maxdepth 1 -name 'test_*.sh' | wc -l)
py=$(find "$here" -maxdepth 1 -name 'test_*.py' | wc -l)
total=$((cpp + sh_ + py))
if [ "$total" -lt 30 ]; then
    bad "only $total test files in tests/raye ($cpp cpp, $sh_ sh, $py py); the package build ran 31 on 2026-11-06, so files have gone missing"
fi

[ "$fail" = 0 ] || exit 1
echo "ok: $n checks, 0 failed ($cpp cpp + $sh_ sh + $py py = $total tests registered by the globs)"
