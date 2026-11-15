# SPDX-License-Identifier: GPL-3.0-or-later
"""What Files shows in English is American English. The archive messages
used to say "cancelled"."""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent.parent
only_british = re.compile(r"cancell(?:ed|ing)|licence|colour|favourite|behaviour", re.I)
po = (root / "po/fr/raye-files.po").read_text(encoding="utf-8")
assert po.count("msgid ") > 100, "the check does not read the catalogue"
pot = (root / "po/raye-files.pot").read_text(encoding="utf-8")
bad = [f"{name}:{n}: {l}" for name, text in (("po", po), ("pot", pot))
       for n, l in enumerate(text.splitlines(), 1) if l.startswith("msgid ") and only_british.search(l)]
for path in (root / "src/raye").rglob("*.cpp"):
    for n, l in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if re.search(r'\bi18n[cp]{0,2}\(', l) and only_british.search(l):
            bad.append(f"{path.relative_to(root)}:{n}: {l.strip()}")
if bad:
    print("British spelling shown to people:\n" + "\n".join(bad))
    sys.exit(1)
print("US spelling: ok")
