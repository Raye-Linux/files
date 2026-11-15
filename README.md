# Files

The file explorer of Raye Linux, package `raye-files`. It is a fork of
KDE's Dolphin: the views, selection, file jobs, panels and plugins stay
Dolphin's, and Raye redraws what is around them: the title row and tabs,
the address row, the command bar, the navigation pane, the status bar and
the menus.

- `src/raye/` everything Raye adds
- `tests/raye/` its tests
- `raye-changes/` one file per change to Dolphin's own files, with the
  reason. Read these before moving to a newer Dolphin.
- `UPSTREAM` which Dolphin release this is based on

## Build

    cmake -S . -B build -G Ninja
    cmake --build build

## Test

    ctest --test-dir build -L raye --output-on-failure

To build the package: `dpkg-buildpackage -b -us -uc`.

## License

GPL-2.0-or-later like Dolphin (`COPYING`); other licenses used by some
files are in `LICENSES/`.

## Links

- Website: https://rayelinux.com
- Discord: https://discord.gg/TrxhUkabyB (questions, bug reports, news)
