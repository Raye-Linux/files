#!/bin/sh
# The rules that hide the system's own partitions (data/system), checked
# without a running udev: a small evaluator reads 90-raye-hide-system.rules
# the way udev does and runs it on property sets taken from real installs.
# The helper the rules call is run for real, on a fake sysfs tree. The list
# behind /.hidden is checked too. Needs sh and awk only; when udevadm is
# there, "udevadm verify" checks the syntax on top.
set -eu
src="${RAYE_FILES_SOURCE_DIR:?}/data/system"
rules="$src/90-raye-hide-system.rules"
helper="$src/raye-is-system-dm"
hidden="$src/dot-hidden"
cmake="$src/CMakeLists.txt"

fail=0
bad() { echo "FAIL: $*"; fail=1; }

for f in "$rules" "$helper" "$hidden" "$cmake"; do
    [ -f "$f" ] || { echo "missing $f"; exit 1; }
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# --- The evaluator ---------------------------------------------------------
# It knows the keys these rules use and nothing else: anything it cannot
# read is an error, so a rule it does not understand is never "passed".
cat > "$work/eval.awk" <<'AWK'
function die(msg) { print "evaluator: " msg > "/dev/stderr"; broken = 1; exit 3 }
function glob2re(p,    n, k, i, j, c, a, r, out, alts) {
    n = split(p, alts, "|")
    out = ""
    for (k = 1; k <= n; k++) {
        a = alts[k]; r = ""
        for (i = 1; i <= length(a); i++) {
            c = substr(a, i, 1)
            if (c == "*") r = r ".*"
            else if (c == "?") r = r "."
            else if (c == "[") {
                j = index(substr(a, i), "]")
                if (j == 0) die("unclosed [ in \"" p "\"")
                r = r substr(a, i, j); i += j - 1
            } else if (index("\\.^$+(){}", c)) r = r "\\" c
            else r = r c
        }
        out = out (k > 1 ? "|" : "") r
    }
    if (n == 0) out = ""
    return "^(" out ")$"
}
function value(key,    name) {
    if (key == "KERNEL" || key == "SUBSYSTEM" || key == "ACTION") return dev[key]
    name = key; sub(/^ENV\{/, "", name); sub(/\}$/, "", name)
    return env[name]
}
function run(cmd,    c) {
    c = cmd
    gsub(/%k/, dev["KERNEL"], c)
    if (index(c, "/usr/lib/raye/udev/raye-is-system-dm ") != 1)
        die("unknown program: " cmd)
    c = helper " " substr(c, length("/usr/lib/raye/udev/raye-is-system-dm ") + 1)
    return system(c " >/dev/null 2>&1") == 0
}
function parse(s,    q) {
    nt = 0
    while (1) {
        sub(/^[ \t,]+/, "", s)
        if (s == "") break
        if (!match(s, /^[A-Z_]+(\{[A-Za-z0-9_]+\})?/)) die("cannot read: " s)
        nt++; tk[nt] = substr(s, 1, RLENGTH); s = substr(s, RLENGTH + 1)
        sub(/^[ \t]+/, "", s)
        if (!match(s, /^(==|!=|\+=|:=|=)/)) die("no operator after " tk[nt])
        top[nt] = substr(s, 1, RLENGTH); s = substr(s, RLENGTH + 1)
        sub(/^[ \t]+/, "", s)
        if (substr(s, 1, 1) != "\"") die("value of " tk[nt] " not quoted")
        s = substr(s, 2); q = index(s, "\"")
        if (q == 0) die("unclosed value of " tk[nt])
        tval[nt] = substr(s, 1, q - 1); s = substr(s, q + 1)
        if (s != "" && s !~ /^[ \t]*,/) die("no comma after " tk[nt])
    }
}
function apply(    i, k, ok, na, goto_to) {
    ok = 1; na = 0; goto_to = ""
    for (i = 1; i <= nt && ok; i++) {
        k = tk[i]
        if (k == "LABEL") {
            if (nt != 1) die("LABEL with other keys")
            labels[tval[i]] = 1
            if (skipping == tval[i]) skipping = ""
            return
        }
        if (skipping != "") return
        if (k == "KERNEL" || k == "SUBSYSTEM" || k == "ACTION" || k ~ /^ENV\{/) {
            if (top[i] == "==") ok = value(k) ~ glob2re(tval[i])
            else if (top[i] == "!=") ok = value(k) !~ glob2re(tval[i])
            else if (top[i] == "=" && k ~ /^ENV\{/) { na++; an[na] = k; av[na] = tval[i] }
            else die("operator " top[i] " on " k)
        } else if (k == "PROGRAM" && top[i] == "=") ok = run(tval[i])
        else if (k == "GOTO" && top[i] == "=") goto_to = tval[i]
        else die("key not known here: " k top[i])
    }
    if (skipping != "" || !ok) return
    for (i = 1; i <= na; i++) { k = an[i]; sub(/^ENV\{/, "", k); sub(/\}$/, "", k); env[k] = av[i] }
    if (goto_to != "") { skipping = goto_to; gotos[goto_to] = 1 }
}
BEGIN {
    while ((getline line < props) > 0) {
        eq = index(line, "="); if (eq == 0) continue
        k = substr(line, 1, eq - 1); v = substr(line, eq + 1)
        if (k == "KERNEL" || k == "SUBSYSTEM" || k == "ACTION") dev[k] = v
        else env[k] = v
    }
    skipping = ""
}
{
    line = $0
    if (pending != "") { line = pending " " line; pending = "" }
    if (line ~ /\\$/) { pending = substr(line, 1, length(line) - 1); next }
    sub(/^[ \t]+/, "", line)
    if (line == "" || substr(line, 1, 1) == "#") next
    parse(line)
    apply()
}
END {
    if (broken) exit 3
    if (pending != "") die("file ends with a continuation")
    for (g in gotos) if (!(g in labels)) die("GOTO to a missing LABEL " g)
    print "UDISKS_IGNORE=" env["UDISKS_IGNORE"]
}
AWK

# --- A fake sysfs and udev database for the helper --------------------------
root="$work/root"
blk() { # blk NAME UEVENT-LINES... : a block device under the fake /sys
    d="$root/sys/class/block/$1"; shift
    mkdir -p "$d/slaves"
    : > "$d/uevent"
    for l in "$@"; do printf '%s\n' "$l" >> "$d/uevent"; done
}
slave() { mkdir -p "$root/sys/class/block/$1/slaves/$2"; }
mkdir -p "$root/run/udev/data" "$root/proc/self"

# Encrypted install: nvme0n1p2 "raye" holds the LUKS container, dm-0 is it unlocked.
blk nvme0n1p2 MAJOR=259 MINOR=2 DEVNAME=nvme0n1p2 DEVTYPE=partition PARTN=2 PARTNAME=raye
blk dm-0 MAJOR=252 MINOR=0 DEVNAME=dm-0 DEVTYPE=disk
slave dm-0 nvme0n1p2
# An encrypted USB stick, partition named "Backup".
blk sdb1 MAJOR=8 MINOR=17 DEVNAME=sdb1 DEVTYPE=partition PARTN=1 PARTNAME=Backup
blk dm-1 MAJOR=252 MINOR=1 DEVNAME=dm-1 DEVTYPE=disk
slave dm-1 sdb1
# A partition whose name only udev knows (no PARTNAME in the uevent).
blk sda3 MAJOR=8 MINOR=3 DEVNAME=sda3 DEVTYPE=partition PARTN=3
printf 'S:disk/by-partlabel/raye\nE:ID_PART_ENTRY_NAME=raye\nE:ID_FS_TYPE=crypto_LUKS\n' > "$root/run/udev/data/b8:3"
blk dm-2 MAJOR=252 MINOR=2 DEVNAME=dm-2 DEVTYPE=disk
slave dm-2 sda3
# A mapper on a mapper: dm-3 on dm-0.
blk dm-3 MAJOR=252 MINOR=3 DEVNAME=dm-3 DEVTYPE=disk
slave dm-3 dm-0
# An old disk without partition names: the unlocked root is found by what
# is mounted on "/".
blk sdc2 MAJOR=8 MINOR=34 DEVNAME=sdc2 DEVTYPE=partition PARTN=2
blk dm-4 MAJOR=252 MINOR=4 DEVNAME=dm-4 DEVTYPE=disk
slave dm-4 sdc2
mkdir -p "$root/sys/class/block/dm-4/dm"
echo luks-0f1e2d3c-aaaa-4bbb-8ccc-123456789abc > "$root/sys/class/block/dm-4/dm/name"
# A partition whose name only starts like Raye's.
blk sdd1 MAJOR=8 MINOR=49 DEVNAME=sdd1 DEVTYPE=partition PARTN=1 PARTNAME=raye_old
blk dm-6 MAJOR=252 MINOR=6 DEVNAME=dm-6 DEVTYPE=disk
slave dm-6 sdd1
# A mapper whose slave loops back on itself must still end.
blk dm-5 MAJOR=252 MINOR=5 DEVNAME=dm-5 DEVTYPE=disk
slave dm-5 dm-5
# Two mappers on each other: only the depth limit stops this one.
blk dm-7 MAJOR=252 MINOR=7 DEVNAME=dm-7 DEVTYPE=disk
blk dm-8 MAJOR=252 MINOR=8 DEVNAME=dm-8 DEVTYPE=disk
slave dm-7 dm-8
slave dm-8 dm-7
cat > "$root/proc/self/mountinfo" <<'MI'
22 1 0:21 / /sys rw,nosuid,nodev,noexec,relatime shared:7 - sysfs sysfs rw
26 1 0:26 /@ / rw,noatime shared:1 - btrfs /dev/mapper/luks-0f1e2d3c-aaaa-4bbb-8ccc-123456789abc rw,compress=zstd:1
30 26 8:33 / /boot rw,relatime shared:60 - ext4 /dev/sdc1 rw
31 26 0:26 /@home /home rw,noatime shared:61 - btrfs /dev/mapper/luks-0f1e2d3c-aaaa-4bbb-8ccc-123456789abc rw
MI

helper_cmd="sh $helper --root $root"

# --- The helper alone -------------------------------------------------------
expect_helper() { # expect_helper STATUS ARG... ; STATUS is 0 or 1
    want=$1; shift
    got=0
    # A helper that never ends would hang udev: 124 from timeout is a failure.
    timeout 10 $helper_cmd "$@" >/dev/null 2>&1 || got=$?
    [ "$got" = "$want" ] || bad "raye-is-system-dm $*: exit $got, wanted $want"
}
expect_helper 0 dm-0            # on the "raye" partition
expect_helper 1 dm-1            # on a USB stick
expect_helper 0 dm-2            # name from the udev database
expect_helper 0 dm-3            # two levels down
expect_helper 0 dm-4            # mounted on /
expect_helper 1 dm-5            # loops, ends
expect_helper 1 dm-7            # a two-device loop ends too
expect_helper 1 dm-6            # "raye_old" is not Raye's
expect_helper 1 dm-9            # does not exist
expect_helper 1 ''
expect_helper 1 ../../etc
expect_helper 1 dm-0/../dm-1
expect_helper 1 .hidden
expect_helper 1                 # no argument
expect_helper 1 dm-0 dm-1       # too many
# The real helper must not need anything but sh builtins and its files.
sh -n "$helper" || bad "raye-is-system-dm is not valid sh"
head -n 1 "$helper" | grep -qx '#!/bin/sh' || bad "raye-is-system-dm must start with #!/bin/sh"

# --- The rules on property sets ---------------------------------------------
# One case per block: "case NAME EXPECTED", properties, "end". EXPECTED is
# the UDISKS_IGNORE the rules must leave: 1, or - for "not set".
n=0
check_case() {
    n=$((n + 1))
    got=$(awk -v props="$work/props" -v helper="$helper_cmd" -f "$work/eval.awk" "$rules") \
        || { bad "evaluator stopped on $name"; return; }
    got=${got#UDISKS_IGNORE=}
    [ -n "$got" ] || got=-
    [ "$got" = "$want" ] || bad "$name: UDISKS_IGNORE=$got, wanted $want"
}
while IFS= read -r line; do
    case $line in
        'case '*) set -- $line; name=$2; want=$3; : > "$work/props" ;;
        end) check_case ;;
        '') ;;
        *) printf '%s\n' "$line" >> "$work/props" ;;
    esac
done <<'CASES'
case root-plain 1
SUBSYSTEM=block
ACTION=add
KERNEL=nvme0n1p2
ID_PART_ENTRY_SCHEME=gpt
ID_PART_ENTRY_NAME=raye
ID_FS_TYPE=btrfs
end

case root-luks-container 1
SUBSYSTEM=block
ACTION=add
KERNEL=nvme0n1p2
ID_PART_ENTRY_SCHEME=gpt
ID_PART_ENTRY_NAME=raye
ID_FS_TYPE=crypto_LUKS
end

case boot-by-gpt-name 1
SUBSYSTEM=block
ACTION=change
KERNEL=nvme0n1p1
ID_PART_ENTRY_SCHEME=gpt
ID_PART_ENTRY_NAME=raye_boot
ID_FS_TYPE=ext4
end

case boot-by-label 1
SUBSYSTEM=block
ACTION=add
KERNEL=sda1
ID_PART_ENTRY_SCHEME=dos
ID_FS_TYPE=ext4
ID_FS_LABEL=raye_boot
end

case swap-partition 1
SUBSYSTEM=block
ACTION=add
KERNEL=sda5
ID_PART_ENTRY_SCHEME=gpt
ID_FS_TYPE=swap
end

case swap-file-loop 1
SUBSYSTEM=block
ACTION=add
KERNEL=loop0
ID_FS_TYPE=swap
end

case unlocked-root 1
SUBSYSTEM=block
ACTION=change
KERNEL=dm-0
DM_NAME=luks-6c1a9f0e-3d2b-4c8a-9e7f-0a1b2c3d4e5f
ID_FS_TYPE=btrfs
end

case unlocked-root-nameless-disk 1
SUBSYSTEM=block
ACTION=change
KERNEL=dm-4
DM_NAME=luks-0f1e2d3c-aaaa-4bbb-8ccc-123456789abc
ID_FS_TYPE=btrfs
end

case unlocked-usb-stick -
SUBSYSTEM=block
ACTION=change
KERNEL=dm-1
DM_NAME=luks-11112222-3333-4444-5555-666677778888
ID_FS_TYPE=exfat
ID_FS_LABEL=Backup
end

case other-mapper -
SUBSYSTEM=block
ACTION=add
KERNEL=dm-0
DM_NAME=vg-data
ID_FS_TYPE=ext4
end

case usb-stick -
SUBSYSTEM=block
ACTION=add
KERNEL=sdb1
ID_PART_ENTRY_SCHEME=dos
ID_FS_TYPE=vfat
ID_FS_LABEL=KINGSTON
end

case windows-c -
SUBSYSTEM=block
ACTION=add
KERNEL=nvme0n1p3
ID_PART_ENTRY_SCHEME=gpt
ID_PART_ENTRY_NAME=Basic data partition
ID_PART_ENTRY_TYPE=ebd0a0a2-b9e5-4433-87c0-68b6b72699c7
ID_FS_TYPE=ntfs
end

case look-alike-names -
SUBSYSTEM=block
ACTION=add
KERNEL=sdb2
ID_PART_ENTRY_SCHEME=gpt
ID_PART_ENTRY_NAME=raye_data
ID_FS_TYPE=ext4
ID_FS_LABEL=Raye
end

case gpt-name-on-dos-table -
SUBSYSTEM=block
ACTION=add
KERNEL=sdb1
ID_PART_ENTRY_SCHEME=dos
ID_PART_ENTRY_NAME=raye
ID_FS_TYPE=ext4
end

case whole-disk -
SUBSYSTEM=block
ACTION=add
KERNEL=nvme0n1
ID_PART_TABLE_TYPE=gpt
end

case not-a-block-device -
SUBSYSTEM=net
ACTION=add
KERNEL=eth0
ID_PART_ENTRY_SCHEME=gpt
ID_PART_ENTRY_NAME=raye
end

case removed -
SUBSYSTEM=block
ACTION=remove
KERNEL=nvme0n1p2
ID_PART_ENTRY_SCHEME=gpt
ID_PART_ENTRY_NAME=raye
end

case shown-by-udisks-stays-shown 0
SUBSYSTEM=block
ACTION=add
KERNEL=sdb1
ID_PART_TABLE_TYPE=dos
ID_PART_ENTRY_TYPE=0x0
ID_FS_TYPE=iso9660
UDISKS_IGNORE=0
end
CASES
[ "$n" -ge 18 ] || bad "only $n cases ran"

# --- The rules file itself -------------------------------------------------
# Never un-hide: only "1" is ever set.
if grep -o 'UDISKS_IGNORE}="[^"]*"' "$rules" | grep -qv 'UDISKS_IGNORE}="1"'; then
    bad "the rules set UDISKS_IGNORE to something else than 1"
fi
# The helper path the rules call is where CMake installs it, and the rules
# go after udisks' own (80-udisks2.rules).
grep -q 'PROGRAM="/usr/lib/raye/udev/raye-is-system-dm %k"' "$rules" \
    || bad "the rules do not call /usr/lib/raye/udev/raye-is-system-dm %k"
grep -q 'lib/raye/udev' "$cmake" || bad "CMake does not install the helper to lib/raye/udev"
grep -q 'lib/udev/rules.d' "$cmake" || bad "CMake does not install the rules to lib/udev/rules.d"
case $(basename "$rules") in 9[0-9]-*) ;; *) bad "the rules must sort after 80-udisks2.rules" ;; esac

if command -v udevadm >/dev/null 2>&1; then
    udevadm verify --resolve-names=never "$rules" >"$work/verify" 2>&1 \
        || { cat "$work/verify"; bad "udevadm verify"; }
else
    echo "note: udevadm not installed, syntax checked by the evaluator only"
fi

# --- /.hidden --------------------------------------------------------------
# One name per line, nothing else: KIO reads every line as a file name.
for name in bin boot dev etc lib lib64 proc root run sbin snap srv sys tmp usr var \
            lost+found .snapshots cdrom swap.img swap swapfile; do
    grep -qxF "$name" "$hidden" || bad ".hidden does not list $name"
done
for name in home media mnt opt; do
    grep -qxF "$name" "$hidden" && bad ".hidden hides $name, which must stay visible"
done
grep -qn '^$\|^#\|/\|^[[:space:]]\|[[:space:]]$' "$hidden" \
    && bad ".hidden has a blank line, a comment, a slash or stray spaces"
[ -z "$(sort "$hidden" | uniq -d)" ] || bad ".hidden lists a name twice"
[ "$(tail -c 1 "$hidden" | od -An -c | tr -d ' ')" = '\n' ] || bad ".hidden does not end with a newline"

[ "$fail" = 0 ] || exit 1
echo "ok: $n rule cases, helper, .hidden"
