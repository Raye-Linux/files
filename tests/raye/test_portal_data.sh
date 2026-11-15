#!/bin/sh
# The files that make the system use raye-files-portal have to agree with each
# other and with the binary: a wrong name here means the desktop's own dialog,
# with no error anywhere.
set -eu
src="${RAYE_FILES_SOURCE_DIR:?}"
data="$src/data/portal"
main="$src/src/raye/portal/main.cpp"
fail=0
check() {
    if ! grep -qF -- "$2" "$1"; then
        echo "$1 lacks: $2"
        fail=1
    fi
}

name=org.freedesktop.impl.portal.desktop.rayefiles

# The name on the bus is the same everywhere.
check "$main" "\"$name\""
check "$data/raye-files.portal" "DBusName=$name"
check "$data/$name.service.in" "Name=$name"
check "$data/raye-files-portal.service.in" "BusName=$name"
check "$data/$name.service.in" "SystemdService=raye-files-portal.service"

# Both portals are offered, and the preference names this backend first with
# the desktop's after it: a broken Raye dialog must fall back, never fail.
check "$data/raye-files.portal" "org.freedesktop.impl.portal.FileChooser"
check "$data/raye-files.portal" "org.freedesktop.impl.portal.AppChooser"
check "$data/portals.conf" "org.freedesktop.impl.portal.FileChooser=raye-files;kde"
check "$data/portals.conf" "org.freedesktop.impl.portal.AppChooser=raye-files;kde"
# The stock list's lines are kept: this file replaces it.
check "$data/portals.conf" "org.freedesktop.impl.portal.Secret=kwallet"
check "$data/portals.conf" "org.freedesktop.impl.portal.Settings=kde;gtk"

# Apps go to the portal, and the backend never to itself.
check "$data/60-raye-file-dialogs.conf" "PLASMA_INTEGRATION_USE_PORTAL=1"
check "$data/raye-files-portal.service.in" "Environment=PLASMA_INTEGRATION_USE_PORTAL=0"
check "$main" 'qputenv("PLASMA_INTEGRATION_USE_PORTAL", "0")'

# A file no app opens goes to the Raye dialog, and that app stays out of menus.
open="$data/org.raye.files.openwith.desktop.in"
check "$open" "MimeType=application/octet-stream;"
check "$open" "NoDisplay=true"
check "$open" "--open-with %U"
check "$src/src/raye/menu/menuactions.cpp" "org.raye.files.openwith"

# The package takes every one of them: dh_missing fails on the rest.
rules="$src/debian/rules"
check "$rules" "usr/lib/environment.d"
check "$rules" "usr/libexec"
check "$rules" "usr/share/xdg-desktop-portal"
check "$rules" "etc/xdg"

# The test that drives the portal over a bus has to be able to run.
check "$src/debian/control" "dbus-daemon"

if [ "$fail" != 0 ]; then
    exit 1
fi
echo ok
