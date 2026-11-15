# SPDX-License-Identifier: GPL-2.0-or-later
"""Every string Raye shows a person in Files must be translatable, and French.

    python3 -I -B tests/raye/test_i18n.py

This is the translation sweep of the store repository, brought here. It is
NOT the whole of it: two of its four defects are already covered in this
repository, better than it covers them, and redoing them would be two
checks that fail together and tell you the same thing twice.

WHERE THE FRONTIER IS, because most of this repository is not ours

src/ is Dolphin 25.12.3, imported whole (see UPSTREAM). Its strings are
KDE's and are translated by KDE's translators, per language, upstream. A
test that asked for French on all of src/ would be asking us to translate
somebody else's project, and it would be switched off within the week.

So the frontier is the one Messages.sh already draws, and it is drawn in
exactly one place -- that file -- rather than copied here:

    src/raye/**                         Raye code, all the usual i18n calls
    the rest of src/, i18nd("raye-files", ...) only

Defect (b) below uses the first half, src/raye, which is where a person
looks at something Raye wrote. The second half is a handful of strings
added to upstream files (three files today) and it is checked by the
catalogue, which is the only thing that can see a domain argument.

WHAT THIS FILE CHECKS, AND WHAT IT DELIBERATELY LEAVES ALONE

  (a) marked translatable and no French -- ALREADY COVERED, not redone.
      tests/raye/test_catalog.sh runs `msgcmp po/fr/raye-files.po
      po/raye-files.pot`, which fails on an empty translation AND on a
      fuzzy one, and then regenerates the template with this
      repository's own Messages.sh and diffs it. That is both halves of
      (a) -- the catalogue complete, and the catalogue covering the code
      -- through the frontier's own definition instead of a copy of it.
      ⚠️ A delegation nobody asserts is a hole: class Delegated below
      fails if that script stops doing either of those two things.

  (a3) French using a placeholder the code does not supply. msgfmt does
      not catch this: KDE's %1 is not a printf placeholder, so
      --check-format sees nothing to check, and msgcmp compares keys and
      not contents. Nothing in this repository covered it. Checked here.

  (b) VISIBLE AND NEVER MARKED. No translation tool will ever report
      this one -- msgcmp, msgfmt, the translators and every web platform
      read the catalogue, and the string is not in it. Nothing in this
      repository covered it either. It is the reason this file exists.
      Two nets, because shape and position each catch what the other
      misses:
        b1  a literal in a slot that renders text (text:, title:...),
            in QML. Position is the evidence, so a lone word is caught.
        b2  a literal shaped like human prose, wherever it sits.
            Position tells us nothing, so the filter is wide: a single
            token here is an identifier, and only several real words
            count as prose.

  (c) The files gettext never reads: the .desktop entries and the
      AppStream metainfo. They carry their own Name[fr]= and
      xml:lang="fr". test_desktop_file.sh runs desktop-file-validate on
      one of them, which checks that the file is well formed and says
      nothing whatever about translation.

🚨 WHY THE FLOORS AND THE GUARDS EXIST. A test that walks a list and
asserts something about every item PASSES -- green, silent -- when the
list is empty: zero items, zero assertions, nothing said. A renamed
folder, a regex that stops matching, a catalogue cut to its header: each
one empties a list, and this whole file would then report success while
protecting nothing. So every collection reasoned about here is asserted
non-empty, and the large ones against a floor well below today's count.
The guards are tests of their own, in class Guards.

WHAT THIS FILE CANNOT SEE, said plainly rather than left to be assumed

  * A key built while the program runs -- i18n(prefix + suffix) -- is
    invisible to any reader of the source. Nothing here can follow it.
  * Whether a translation is RIGHT, or still matches the English. Only a
    person reading Files in French can tell.
  * Dolphin's own strings, on purpose: see the frontier above.
  * A string marked, translated, and then shown in the wrong place.
  * Anything in a .ui, .rc or .kcfg file. There are none under src/raye
    today; Messages.sh would need `extractrc` for them, and extractrc is
    NOT installed on the machine this was written on -- Messages.sh
    fails loudly in that case rather than dropping the strings, which is
    the right behaviour and is why this is a note and not a check.
  * The render harness's own sample data and captions, skipped by the
    RAYE_SHOWCASE rule below. Those strings reach a generated image a
    developer looks at, never a user's screen.
"""
import pathlib
import re
import unittest

# --------------------------------------------------------------- CONFIG
ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
DOMAIN = "raye-files"
# What a person looks at and Raye wrote (defect b). NOT all of src/:
# the rest is Dolphin's, translated upstream.
INTERFACE_DIRS = ("src/raye",)
# The files gettext never reads (defect c).
DESKTOP_GLOBS = ("src/*.desktop",)
APPSTREAM_GLOBS = ("src/*.appdata.xml", "src/*.metainfo.xml")
QML_SUFFIXES = (".qml", ".js", ".mjs")
CODE_SUFFIXES = (".cpp", ".h", ".cc")

# Today: 545 catalogue entries, 200 interface files, 3381 literals, 1297
# marked, 183 in a visible slot, 2 desktop files, 1 metainfo, 60
# showcase blocks. A floor catches a collapse to nothing; it is not
# meant to be raised each time a string is added.
FLOORS = {"catalogue": 400, "interface_files": 120, "literals": 1500,
          "marked": 700, "visible_slots": 60, "desktop_files": 2,
          "appstream_files": 1, "showcase_blocks": 20}

PO = ROOT / "po" / "fr" / f"{DOMAIN}.po"
POT = ROOT / "po" / f"{DOMAIN}.pot"
CATALOGUE_SCRIPT = ROOT / "tests" / "raye" / "test_catalog.sh"

# Slots that put their value on the screen.
VISIBLE_SLOTS = {
    "text", "title", "subtitle", "message", "caption", "actionText",
    "placeholderText", "description", "notice", "toolTip", "displayText",
    "label", "header", "footer", "errorText", "hint", "progressText",
    "ToolTip.text", "Accessible.name", "Accessible.description",
}
# Slots whose value is a technical name, never prose: "Inter Display" is
# a typeface, so b2 must not ask anyone to translate it.
FONT_SLOTS = {"family", "font.family", "fontFamily"}

# ------------------------------------------------------- what is not text
#
# Six rules, and every one of them is here because it was MEASURED
# against this repository, not guessed. Together they take the b2 pool
# from 272 findings to the handful at the bottom of this block. The
# alternative was an exception list with 272 entries in it, and a
# guard with 272 exceptions makes noise, gets ignored, then gets
# switched off.

# 1. KDE's NOOP macros mark a string for the catalogue just as i18n()
#    does -- they are in Messages.sh's keyword list -- but they are
#    SPELLED IN CAPITALS, so the lowercase i18n pattern misses them.
#    src/raye/typenames/typenames.cpp is one table of 100
#    I18NC_NOOP("@item file type", ...) entries: every one of them is
#    marked, translated, and was reported as unmarked prose. 210 of the
#    272. The store repository's copy of this sweep still has the
#    lowercase-only pattern; it has no NOOP macro today, so the hole is
#    latent there rather than absent.
I18N_CALL = re.compile(r"\b(?:[kx]{0,2}i18n[dcp]{0,4}|I18N[A-Z0-9]*_NOOP[0-9]*)\s*\(")

# 2. RAYE_SHOWCASE(id, "Name", "caption") registers an entry of the
#    render harness (src/raye/ui/showcase.h, tests/raye/harness): a
#    widget built with sample data so a screenshot can be taken of it.
#    Its caption and its sample file names reach a generated image a
#    developer looks at. They are not interface text, and translating
#    them would put French into the English screenshots.
#    The macro's ARGUMENTS and its BLOCK both count: the caption is in
#    the parentheses, the sample data is in the braces after them.
SHOWCASE_CALL = ("RAYE_SHOWCASE",)
# 2b. The same registration written as a function call. The macro is
#    sugar over Raye::Ui::Showcase::add(name, description, builder), and
#    src/raye/preview/previewshowcase.cpp calls it directly, with the
#    builder as a lambda argument -- so the whole call, sample data
#    included, is inside one pair of parentheses. `add` is far too
#    common a name to ignore everywhere, so it only counts in a file
#    that has imported THIS add: the trigger is the using-declaration.
SHOWCASE_IMPORT = re.compile(r"\bUi::Showcase::add\b")
SHOWCASE_FUNCTION = ("add",)
# 2c. demoText("English", "French") is the harness's own translation:
#    it reads KLocalizedString::languages() and returns one or the
#    other, so the sample tab really does say "Photos de vacances" in
#    the French screenshots. Both arguments are already both languages.
#    Marking them would put screenshot sample data in the catalogue and
#    ask a translator to translate "Vacation photos 2025 Lisbon" twice.
SAMPLE_CALLS = ("demoText",)

# 3. Qt's categorised logging streams its text INSTEAD of passing it:
#    qCWarning(RayeArchiveLog) << "refusing entry" << name. The literal
#    is outside the parentheses, so reading the call's arguments finds
#    nothing -- the whole statement has to be taken.
LOG_CALLS = ("qDebug", "qInfo", "qWarning", "qCritical", "qFatal",
             "qCDebug", "qCInfo", "qCWarning", "qCCritical", "qCFatal",
             "qmlWarning", "qmlInfo", "Q_ASSERT", "Q_ASSERT_X")

# 4. Calls that MATCH a string instead of showing it: the literal is a
#    value being looked for, like the right-hand side of ===.
MATCH_CALLS = ("indexOf", "lastIndexOf", "startsWith", "endsWith",
               "includes", "contains", "localeCompare", "split", "search",
               "compare", "hasOwnProperty", "section", "mid", "remove",
               "any_of", "none_of", "all_of", "matches")

# 5. Calls that NAME something instead of showing it: a configuration
#    group or key, a D-Bus member, a plugin id, a Qt object name, a
#    typeface. "only basic indexing" is a key in baloofilerc and
#    "Inter Display" is a typeface; neither is a sentence anyone reads.
NAME_CALLS = ("group", "readEntry", "writeEntry", "hasKey", "deleteEntry",
              "readXdgListEntry", "writeXdgListEntry", "openStateConfig",
              "setObjectName", "objectName", "findPluginById",
              "setFamily", "setFamilies", "QFont", "setStyleName",
              "setArguments", "createFunction", "qobject_cast",
              "setProperty", "property", "connect", "invokeMethod")
# 5b. The same, declared instead of called: `QFont mono(QStringLiteral(
#    "Cascadia Mono"));` constructs a QFont with a typeface name, and
#    the call head is the variable, not the type. A font family is never
#    prose -- a translated typeface name silently falls back to the
#    default font, which is a bug nobody sees until a screenshot.
NAME_DECLARATIONS = re.compile(r"\bQFont\s+\w+\s*\(")

# 6. Qt macros whose strings are names for the metaobject, not text.
META_CALLS = ("Q_CLASSINFO", "Q_PROPERTY", "Q_ENUM", "Q_ENUMS", "Q_INVOKABLE",
              "qRegisterMetaType", "qDBusRegisterMetaType", "Q_DECLARE_METATYPE",
              "setApplicationDomain", "setApplicationName", "setOrganizationName",
              "Q_COREAPP_STARTUP_FUNCTION", "Q_DECLARE_LOGGING_CATEGORY",
              "Q_LOGGING_CATEGORY")

# Every exception carries its reason. A long list here means a rule is
# wrong and should be fixed instead of papered over. There are three.
ALLOWED_UNMARKED = {
    # (path, literal): reason it is not shown to anyone
    ("src/raye/preview/previewshowcase.cpp", "Meeting notes.txt"):
        "the render harness writes this file into a temporary folder so the "
        "preview pane has something to preview; it is sample data at namespace "
        "scope, outside any RAYE_SHOWCASE block, and the name is what appears "
        "in the English screenshot",
    ("src/raye/preview/previewshowcase.cpp", "Sample font.ttf"):
        "the same temporary folder, for the font preview",
    ("src/raye/locations/locationnames.cpp", "This PC"):
        "the first column of a {id, translated name, url} table: an "
        "identifier for the page, beside thisPcCrumb().text which IS the "
        "translated name the user reads",
    ("src/raye/locations/locationnames.cpp", "Recycle Bin"):
        "the same table, same column",
    ("src/raye/titlebar/titlebar.cpp", "Windeco Exception %1"):
        "a group name of rayerc, the decoration's config file",
    ("src/raye/titlebar/titlebar.h", "dolphin org.kde.dolphin"):
        "KWin's window class of Files, matched against the decoration's "
        "exceptions",
    ("src/raye/window/shortcuts.cpp", "activate_tab_%1"):
        "the object name of an action, never shown",
    ("src/raye/netdrive/networkdrives.cpp", "Raye network drives"):
        "a folder name inside the wallet, never shown by Files",
}

# Whole files nobody reads. Keep this short: each one is a promise that
# nothing in it ever reaches a window.
NEVER_SHOWN = {
    "src/raye/netdrive/helper/main.cpp":
        "the root helper of Map network drive: its messages go to stderr, "
        "Files reads only its exit code and shows its own words",
}


# ------------------------------------------------------- reading the .po
def parse_po(text):
    """(msgctxt, msgid) -> the whole entry, for the live entries.

    Obsolete entries (#~) are left out: they describe strings the code
    no longer has, so an empty one is not a missing translation.
    """
    entries = {}
    cur = {}
    field = None

    def flush():
        if "msgid" in cur:
            entries[(cur.get("msgctxt", ""), cur["msgid"])] = dict(cur)
        cur.clear()

    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("#~"):
            flush()
            field = None
            continue
        if not line:
            field = None   # the blank line that separates two entries
            continue
        if line.startswith("#"):
            continue
        head = re.match(r'(msgctxt|msgid_plural|msgid|msgstr(?:\[\d+\])?)\s+"(.*)"$', line)
        if head:
            key, value = head.group(1), head.group(2)
            if (key == "msgctxt" and cur) or (key == "msgid" and "msgid" in cur):
                flush()
            cur[key] = value
            field = key
            continue
        more = re.match(r'"(.*)"$', line)
        if more and field:
            cur[field] = cur.get(field, "") + more.group(1)
    flush()
    entries.pop(("", ""), None)   # the header: an empty msgid, not a string
    return entries


def placeholders(text):
    return set(re.findall(r"%\d+", text))


# ------------------------------------------- reading the interface code
LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"' r"|'(?:[^'\\\n]|\\.)*'")
SLOT = re.compile(r"\b([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\s*:(?!:)")
COMPARE_BEFORE = re.compile(r"(===|!==|==|!=)\s*$")
COMPARE_AFTER = re.compile(r"^\s*(===|!==|==|!=)")


def source_files(dirs):
    return sorted(
        p for d in dirs if (ROOT / d).exists()
        for p in (ROOT / d).rglob("*")
        if p.suffix in QML_SUFFIXES + CODE_SUFFIXES
    )


def strip_comments(text):
    """Comments go first: a comment quoting an English string must not
    fail a test whose job is to protect that documentation."""
    # the newlines of a block comment are kept, so the line numbers this
    # test reports are the line numbers of the file
    text = re.sub(r"/\*.*?\*/",
                  lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    out = []
    for line in text.split("\n"):
        cut, i, n, quote = None, 0, len(line), None
        while i < n:
            c = line[i]
            if quote:
                if c == "\\":
                    i += 1
                elif c == quote:
                    quote = None
            elif c in "\"'":
                quote = c
            elif c == "/" and i + 1 < n and line[i + 1] == "/":
                cut = i
                break
            i += 1
        out.append(line if cut is None else line[:cut])
    return "\n".join(out)


def _skip_quote(text, i, n):
    """Past the string literal that starts at i."""
    quote, i = text[i], i + 1
    while i < n and text[i] != quote:
        i += 2 if text[i] == "\\" else 1
    return i


def _heads(text, names):
    if isinstance(names, re.Pattern):
        return [(m.start(), m.end() - 1) for m in names.finditer(text)]
    return [(m.start(), m.end() - 1)
            for m in re.finditer(r"\b(?:%s)\s*\(" % "|".join(names), text)]


def _matching(text, paren):
    """The index of the ) that closes the ( at `paren`."""
    i, depth, n = paren, 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            i = _skip_quote(text, i, n)
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return i


def call_spans(text, names):
    """Character ranges covered by a call to one of `names`."""
    return [(begin, _matching(text, paren)) for begin, paren in _heads(text, names)]


def statement_spans(text, names):
    """A call plus the rest of its statement, for the ones that STREAM
    their text instead of passing it: qCWarning(cat) << "..." << x;"""
    spans, n = [], len(text)
    for begin, paren in _heads(text, names):
        i = _matching(text, paren) + 1
        while i < n:
            c = text[i]
            if c in "\"'":
                i = _skip_quote(text, i, n)
            elif c in ";{}":
                break
            i += 1
        spans.append((begin, min(i, n - 1)))
    return spans


def block_spans(text, names):
    """A call plus the braced block that follows it, for a macro that
    opens one: RAYE_SHOWCASE(id, "Name", "caption") { ... }"""
    spans, n = [], len(text)
    for begin, paren in _heads(text, names):
        i = _matching(text, paren) + 1
        while i < n and text[i] in " \t\r\n":
            i += 1
        if i >= n or text[i] != "{":
            spans.append((begin, min(i, n - 1)))   # no block: the call alone
            continue
        depth = 0
        while i < n:
            c = text[i]
            if c in "\"'":
                i = _skip_quote(text, i, n)
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        spans.append((begin, min(i, n - 1)))
    return spans


class Found:
    """One string literal, where it is and what it lands in."""
    __slots__ = ("path", "line", "value", "slot")

    def __init__(self, path, line, value, slot):
        self.path, self.line, self.value, self.slot = path, line, value, slot

    def __str__(self):
        return f"{self.path}:{self.line}: {self.slot or '(no slot)'}: {self.value!r}"


def scan_interface():
    """Every literal of the interface code, with its slot, split into
    the ones a translation call covers and the ones it does not."""
    marked, unmarked, files, showcases = [], [], [], 0
    for path in source_files(INTERFACE_DIRS):
        rel = str(path.relative_to(ROOT))
        files.append(rel)
        text = strip_comments(path.read_text(encoding="utf-8"))
        translated = call_spans(text, I18N_CALL)
        showcase = block_spans(text, SHOWCASE_CALL)
        if SHOWCASE_IMPORT.search(text):
            showcase += call_spans(text, SHOWCASE_FUNCTION)
        showcases += len(showcase)
        ignored = (statement_spans(text, LOG_CALLS)
                   + call_spans(text, MATCH_CALLS)
                   + call_spans(text, NAME_CALLS)
                   + call_spans(text, NAME_DECLARATIONS)
                   + call_spans(text, META_CALLS)
                   + call_spans(text, SAMPLE_CALLS)
                   + showcase)
        slots = [(m.start(), m.group(1)) for m in SLOT.finditer(text)]
        for m in LITERAL.finditer(text):
            at, end = m.start(), m.end()
            slot = ""
            for pos, name in slots:
                if pos >= at:
                    break
                slot = name
            item = Found(rel, text.count("\n", 0, at) + 1, m.group(0)[1:-1], slot)
            if any(a <= at <= b for a, b in translated):
                marked.append(item)
                continue
            if any(a <= at <= b for a, b in ignored):
                continue
            # a literal being compared is a value, not something shown
            if COMPARE_BEFORE.search(text[max(0, at - 40):at]) or \
               COMPARE_AFTER.match(text[end:end + 8]):
                continue
            unmarked.append(item)
    return marked, unmarked, files, showcases


# A literal that is plainly not text, whatever slot it sits in.
NOT_TEXT = [
    r"^\s*$",                       # empty, blank
    r"^.$",                         # one character
    r"^[^A-Za-z]*$",                # punctuation, digits, %1, arrows
    r"^#[0-9a-fA-F]{3,8}$",         # hex colour
    # a URL or a scheme: https://, file://, and raye:home, recentlyused:/
    # -- a scheme with a path after the colon and no space in it
    r"^[a-z][a-z0-9+.-]*:(//|$|[^ ]*$)",
    r"/",                           # a path, or a fragment of one
    # a suffix, or a chain of them: .svg and .tar.zst alike
    r"^\.[A-Za-z0-9]+(\.[A-Za-z0-9]+)*$",
    r"[<>]",                        # markup: <a href=", </a>
]
NOT_TEXT = [re.compile(p) for p in NOT_TEXT]

# A single token is an identifier here, never a sentence. Prose also
# never carries the punctuation of a regex, a Windows path or a
# key=value line: those are what a machine matches, not what a person
# reads.
NOT_PROSE = [
    r"^[A-Za-z_]\w*$",                   # Icons, Compact, Inter, Game
    r"^-?[a-z0-9]+(-[a-z0-9]+)*-?$",     # -filled, kebab-case, raye-command-
    r"^[A-Za-z_][\w-]*(\.[\w+-]+)+$",    # org.kde.dolphin, Qt.AlignLeft
    r"^[A-Za-z][\w.]*-[\w.]+$",          # GPL-2.0-or-later
    r"^(Ctrl|Alt|Shift|Meta)\+",         # Ctrl+F
    r"^[A-Za-z_]\w*(::[A-Za-z_]\w*)+$",  # KParts::ReadOnlyPart
    r"^[A-Za-z_]\w*\([\w,:<>*& ]*\)$",   # openRequested(QUrl), a signature
    r"[\\|=^$*\[\]{}]",                  # regex, glob, Windows path, Key=value
]
NOT_PROSE = [re.compile(p) for p in NOT_PROSE]
WORD = re.compile(r"[A-Za-z]{2,}")


def not_text(value):
    return any(p.search(value) for p in NOT_TEXT)


def looks_like_prose(value):
    """Several real words: what a person reads, not what a machine matches."""
    if not_text(value) or any(p.search(value) for p in NOT_PROSE):
        return False
    return len(WORD.findall(value)) >= 2


def allowed(item):
    return (item.path, item.value) in ALLOWED_UNMARKED or item.path in NEVER_SHOWN


def lines_of(items):
    return sorted(str(i) for i in items)


def glob_all(globs):
    return sorted({p for g in globs for p in ROOT.glob(g)})


CATALOGUE = parse_po(PO.read_text(encoding="utf-8")) if PO.exists() else {}
MARKED, UNMARKED, FILES, SHOWCASES = scan_interface()
IN_VISIBLE_SLOT = [i for i in MARKED + UNMARKED
                   if i.path.endswith(QML_SUFFIXES) and i.slot in VISIBLE_SLOTS]


class Guards(unittest.TestCase):
    """What stops this file from passing by having looked at nothing."""

    def test_the_catalogue_was_read(self):
        self.assertTrue(PO.exists(), f"{PO} is missing: French is the first language")
        self.assertGreaterEqual(
            len(CATALOGUE), FLOORS["catalogue"],
            f"only {len(CATALOGUE)} entries read from po/fr/{DOMAIN}.po: the "
            f"catalogue or parse_po collapsed, and a3 would pass on nothing")

    def test_the_interface_was_scanned(self):
        self.assertGreaterEqual(
            len(FILES), FLOORS["interface_files"],
            f"only {len(FILES)} interface files found under {INTERFACE_DIRS}: "
            f"the Raye folder was renamed, and (b) is asserting about nothing")
        self.assertGreaterEqual(
            len(MARKED) + len(UNMARKED), FLOORS["literals"],
            f"the literal scanner found {len(MARKED) + len(UNMARKED)} literals: "
            f"LITERAL no longer matches")

    def test_translation_calls_are_recognised(self):
        # If I18N_CALL stopped matching, every marked string would look
        # unmarked and b1/b2 would drown in noise. If it matched too much,
        # they would go quiet instead -- that is the dangerous direction.
        self.assertGreaterEqual(
            len(MARKED), FLOORS["marked"],
            f"only {len(MARKED)} literals seen inside a translation call")

    def test_the_noop_macros_are_recognised(self):
        # The capitalised NOOP macros were the single biggest source of
        # noise before they were understood: 210 findings, every one of
        # them a string that was marked and translated all along. If this
        # drops to nothing, either the table moved or the pattern broke.
        noop = [i for i in MARKED if i.path.endswith(".cpp")
                and i.value.startswith("@item file type")]
        self.assertTrue(
            noop, "no I18NC_NOOP string recognised as marked: I18N_CALL no "
                  "longer matches the capitalised NOOP macros, and b2 is about "
                  "to report a hundred translated strings as untranslated")

    def test_the_showcase_reader_works(self):
        # The render harness's sample data is skipped by this rule. If the
        # rule stopped finding the blocks, b2 would fill with screenshot
        # captions; if it swallowed the file, b2 would go silent. Both
        # directions are bounded: a floor here, and the marked floor above.
        self.assertGreaterEqual(
            SHOWCASES, FLOORS["showcase_blocks"],
            f"only {SHOWCASES} RAYE_SHOWCASE blocks found: the macro was "
            f"renamed, and b2 is about to report the harness's captions")

    def test_the_slot_reader_works(self):
        # b1 says nothing at all if no literal is ever attributed to a
        # slot that shows text.
        self.assertGreaterEqual(
            len(IN_VISIBLE_SLOT), FLOORS["visible_slots"],
            f"only {len(IN_VISIBLE_SLOT)} literals landed in one of "
            f"{sorted(VISIBLE_SLOTS)}: SLOT or VISIBLE_SLOTS is wrong, and b1 "
            f"is asserting about an empty set")

    def test_b2_has_something_to_look_at(self):
        pool = [i for i in UNMARKED if i.slot not in FONT_SLOTS]
        self.assertTrue(
            pool, "no unmarked literal outside a font slot: b2 is asserting "
                  "about an empty set")

    def test_the_data_files_are_all_checked(self):
        desktop = glob_all(DESKTOP_GLOBS)
        self.assertGreaterEqual(
            len(desktop), FLOORS["desktop_files"],
            f"only {len(desktop)} desktop files matched {DESKTOP_GLOBS}: {desktop}")
        appstream = glob_all(APPSTREAM_GLOBS)
        self.assertGreaterEqual(
            len(appstream), FLOORS["appstream_files"],
            f"only {len(appstream)} metainfo files matched {APPSTREAM_GLOBS}")


class Delegated(unittest.TestCase):
    """Defect (a) is checked by tests/raye/test_catalog.sh, not here.

    A delegation nobody asserts is a hole: the day that script loses its
    msgcmp, nothing fails, and the catalogue silently stops being
    checked at all -- there would be no test of (a) anywhere, and this
    file's docstring would still say there was one."""

    def script(self):
        self.assertTrue(
            CATALOGUE_SCRIPT.is_file(),
            f"{CATALOGUE_SCRIPT.relative_to(ROOT)} is gone: defect (a) -- a "
            f"string marked translatable with no French -- is now checked by "
            f"NOTHING. Either bring it back or check (a) here.")
        return CATALOGUE_SCRIPT.read_text(encoding="utf-8")

    def test_something_still_compares_the_catalogue_to_the_template(self):
        self.assertIn(
            "msgcmp", self.script(),
            "test_catalog.sh no longer runs msgcmp: an empty or fuzzy French "
            "translation is no longer caught anywhere")

    def test_something_still_rebuilds_the_template_from_the_sources(self):
        text = self.script()
        # The script must RUN it, not merely name it. Checking for the
        # word alone passed on a script whose only remaining mention was
        # `echo "Messages.sh failed"` -- measured, 2026-11-06, while
        # proving this very class could go red. That is the same shape as
        # the bug recorded in test_desktop_file.sh, where a missing file
        # slipped past a ': error:' filter and the script printed "ok".
        self.assertRegex(
            text, r"(?m)(^|[\s;&(|])\./Messages\.sh\b",
            "test_catalog.sh no longer RUNS ./Messages.sh: a string the code "
            "has gained and the catalogue never heard of is no longer caught "
            "anywhere, and it reaches the screen in English with no translator "
            "ever told")
        self.assertTrue(
            (ROOT / "Messages.sh").is_file(),
            "Messages.sh is gone: it is the single definition of the frontier "
            "between Raye's strings and Dolphin's, and both that script and "
            "this file's INTERFACE_DIRS lean on it")

    def test_the_template_exists_for_it_to_compare_against(self):
        self.assertTrue(POT.is_file(), f"{POT} is missing")


class FrenchFitsTheCode(unittest.TestCase):
    """Defect (a3): msgcmp compares keys, msgfmt cannot check KDE's %1."""

    def test_french_fills_only_placeholders_the_code_supplies(self):
        """A French form using %2 where the call passes one argument shows
        "%2" to the reader."""
        bad = []
        for (ctx, msgid), entry in CATALOGUE.items():
            # a plural call supplies the placeholders of either form
            supplied = placeholders(msgid) | placeholders(entry.get("msgid_plural", ""))
            for key in sorted(k for k in entry if k.startswith("msgstr")):
                extra = placeholders(entry[key]) - supplied
                if extra:
                    bad.append(f"{msgid!r} {key}={entry[key]!r} uses "
                               f"{sorted(extra)}, which nothing supplies")
        self.assertEqual(
            [], sorted(bad),
            f"{len(bad)} French string(s) asking for an argument the code does "
            f"not pass:\n" + "\n".join("    " + b for b in sorted(bad)))


class VisibleButNeverMarked(unittest.TestCase):
    """Defect (b): no translation tool will ever report these."""

    def test_b1_a_literal_in_a_slot_that_shows_text(self):
        bad = [i for i in UNMARKED
               if i.path.endswith(QML_SUFFIXES) and i.slot in VISIBLE_SLOTS
               and not not_text(i.value) and not allowed(i)]
        self.assertEqual(
            [], lines_of(bad),
            f"{len(bad)} literal(s) dropped straight into a slot that renders "
            f'text. Wrap each one in i18nc("@context", ...), then rebuild the '
            f"catalogue and translate it:\n"
            + "\n".join("    " + l for l in lines_of(bad)))

    def test_b2_prose_outside_any_translation_call(self):
        bad = [i for i in UNMARKED
               if i.slot not in FONT_SLOTS and looks_like_prose(i.value)
               and not allowed(i)]
        self.assertEqual(
            [], lines_of(bad),
            f"{len(bad)} string(s) shaped like a sentence and never marked. If a "
            f"person reads it, wrap it in i18n; if nobody does, add it to "
            f"ALLOWED_UNMARKED with the reason -- and if several arrive at once, "
            f"they probably share a cause, so look for the rule rather than "
            f"writing the exceptions:\n"
            + "\n".join("    " + l for l in lines_of(bad)))

    def test_the_slot_reader_is_only_trusted_where_it_means_something(self):
        """b1 reads QML only, and that is not a detail.

        In QML `name:` is a property being assigned. In C++ the same
        colon is a label, a ternary, a case, a bit-field or a member
        initialiser list, so the nearest identifier before a literal is
        noise: this repository yields slots like `protected`, `Close`,
        `None` and `FactoryAccess`. Position is evidence in QML and
        nothing in C++, which is why b1 is narrow and b2 is wide."""
        cpp = [i for i in MARKED + UNMARKED if i.path.endswith(CODE_SUFFIXES)]
        self.assertTrue(cpp, "no C++ literal found at all")
        self.assertTrue(
            all(not i.path.endswith(CODE_SUFFIXES) for i in IN_VISIBLE_SLOT),
            "a C++ literal was attributed to a visible slot: b1 is reading "
            "C++ colons as QML properties and is about to report labels, "
            "ternaries and initialiser lists as untranslated interface text")


class StringsOutsideTheCatalogue(unittest.TestCase):
    """Defect (c): the files gettext never reads.

    These are Dolphin's files, kept on purpose -- the desktop id stays
    org.kde.dolphin.desktop so the taskbar launcher, Meta+E and "Show in
    folder" keep working (see README). But Raye REWROTE what they say:
    Name went from Dolphin to Files, GenericName from File Manager to
    File Explorer. English that was changed and French that was not is
    exactly how a desktop entry ends up showing the old name in French
    and the new one in English."""

    DESKTOP_KEYS = ("Name", "GenericName", "Comment", "Keywords")

    @staticmethod
    def has_french(text, key):
        return re.search(r"(?m)^%s\[fr\]\s*=\s*\S" % re.escape(key), text)

    def test_desktop_entries_have_french(self):
        found = glob_all(DESKTOP_GLOBS)
        self.assertTrue(found, "no .desktop file found: DESKTOP_GLOBS is wrong")
        missing = []
        for path in found:
            text = path.read_text(encoding="utf-8")
            # one group at a time: each [Desktop Action x] has its own Name=
            bounds = [m.start() for m in re.finditer(r"(?m)^\[", text)] + [len(text)]
            for start, stop in zip(bounds, bounds[1:]):
                block = text[start:stop]
                head = block.splitlines()[0]
                for key in self.DESKTOP_KEYS:
                    if re.search(r"(?m)^%s\s*=\s*\S" % key, block) and \
                       not self.has_french(block, key):
                        missing.append(
                            f"{path.relative_to(ROOT)} {head}: {key}[fr]= is missing")
        self.assertEqual([], sorted(missing),
                         f"{len(missing)} untranslated desktop key(s):\n"
                         + "\n".join("    " + m for m in sorted(missing)))

    def test_appstream_has_french(self):
        found = glob_all(APPSTREAM_GLOBS)
        self.assertTrue(found, "no metainfo file found: APPSTREAM_GLOBS is wrong")
        missing = []
        for path in found:
            text = path.read_text(encoding="utf-8")
            for tag in ("name", "summary"):
                plain = len(re.findall(r"<%s>" % tag, text))
                french = len(re.findall(r'<%s xml:lang="fr">' % tag, text))
                self.assertTrue(
                    plain, f"{path.relative_to(ROOT)}: no <{tag}> at all, so "
                           f"this check is comparing nothing to nothing")
                if plain > french:
                    missing.append(
                        f"{path.relative_to(ROOT)}: {plain - french} <{tag}> "
                        f'without <{tag} xml:lang="fr">')
        self.assertEqual([], sorted(missing),
                         f"{len(missing)} untranslated AppStream string(s):\n"
                         + "\n".join("    " + m for m in sorted(missing)))


class Reachable(unittest.TestCase):
    """A test the package build never runs is a test the image can ship
    without respecting. Measured on another Raye repository this week:
    debian/rules listed the test folders by hand, missed two, and 346
    tests were never run by the build -- green, with nothing executed.
    So the chain that reaches this very file is asserted, not assumed.

    ⚠️ One link is outside this repository and cannot be asserted from
    here: iso/tools/raye_tests_suites.py names this repository's suites
    one by one -- files/catalog, files/desktop-file, files/shell-guards,
    files/wiring, files/udev-rules -- instead of reading the globs
    below. Until an entry for this file is added there, `raye-tests
    --repo files` does not run it, although the package build does."""

    def test_cmake_registers_every_python_test_by_glob(self):
        cmake = (ROOT / "tests" / "raye" / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn(
            "test_*.py", cmake,
            "tests/raye/CMakeLists.txt no longer globs test_*.py: this file "
            "would never be registered, and ctest would not miss it out loud")
        here = pathlib.Path(__file__).name
        self.assertTrue(
            here.startswith("test_") and here.endswith(".py"),
            f"{here} does not match test_*.py, so nothing runs it")

    def test_the_package_build_runs_the_tests(self):
        rules = (ROOT / "debian" / "rules").read_text(encoding="utf-8")
        self.assertIn(
            "BUILD_TESTING=ON", rules,
            "debian/rules no longer configures with BUILD_TESTING=ON: the "
            "if(BUILD_TESTING) block of CMakeLists.txt registers nothing")
        self.assertIn(
            "CTEST_NO_TESTS_ACTION", rules,
            "debian/rules lost CTEST_NO_TESTS_ACTION=error: ctest exits 0 when "
            "it finds no test at all, so the build would go green empty")
        self.assertIn(
            "-L raye", rules,
            "debian/rules no longer asks ctest for the raye label, which is "
            "the label tests/raye/CMakeLists.txt puts on this file")


if __name__ == "__main__":
    unittest.main(verbosity=1)
