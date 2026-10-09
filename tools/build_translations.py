#!/usr/bin/env python3
"""Build the offline Bible text data for omabiblia.

Outputs (under <repo>/data):
  books.tsv                        index, name, abbr, usfm, testament, chapter_count
  translations/<CODE>.tsv          book_index, chapter, verse, text   (one verse per line)
  translations/translations.tsv    CODE, full name, year, licence, source URL, note, attribution
  translations/NOTICE.md           licensing / attribution

Usage:
  build_translations.py                 download (cached) + convert + verify
  build_translations.py --verify-only   verify the existing output files
  build_translations.py --cache DIR     where downloads are cached

Only the 66 books of the Protestant canon are written; deuterocanonical books
present in some sources (WEB) are dropped. Each translation keeps its own
versification. Empty verses (e.g. verses a translation relegates to a footnote)
are omitted.
"""
import argparse
import html
import json
import os
import re
import sys
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
DATA = os.path.join(REPO, "data")
OUT = os.path.join(DATA, "translations")
KJV_RAW = "https://raw.githubusercontent.com/aruljohn/Bible-kjv/master/"
DEFAULT_CACHE = os.environ.get(
    "OMABIBLIA_CACHE",
    os.path.join(os.environ.get("XDG_CACHE_HOME") or os.path.expanduser("~/.cache"), "omabiblia-build"),
)

# eBible.org VPL book codes (BibleWorks style) in Protestant canon order.
VPL_CODES = (
    "GEN EXO LEV NUM DEU JOS JDG RUT 1SA 2SA 1KI 2KI 1CH 2CH EZR NEH EST JOB PSA PRO "
    "ECC SOL ISA JER LAM EZE DAN HOS JOE AMO OBA JON MIC NAH HAB ZEP HAG ZEC MAL "
    "MAT MAR LUK JOH ACT ROM 1CO 2CO GAL EPH PHI COL 1TH 2TH 1TI 2TI TIT PHM HEB JAM "
    "1PE 2PE 1JO 2JO 3JO JUD REV"
).split()
assert len(VPL_CODES) == 66

# Book names used in the publisher's bsb.txt (where they differ from index.json).
BSB_NAME_ALIASES = {"Psalm": "Psalms"}

EBIBLE = "https://ebible.org/Scriptures/{id}_vpl.zip"

# code, full name, year, licence, source url, note, kind, source id
TRANSLATIONS = [
    ("KJV", "King James Version", "1611", "Public Domain",
     "https://github.com/aruljohn/Bible-kjv",
     "Authorized Version (1769 Oxford text), from the public-domain aruljohn/Bible-kjv JSON",
     "kjv", None),
    ("WEB", "World English Bible", "2020", "Public Domain",
     EBIBLE.format(id="eng-web"),
     "Modern English, uses Yahweh; 2020 stable text from eBible.org, deuterocanon dropped",
     "vpl", "eng-web"),
    ("ASV", "American Standard Version", "1901", "Public Domain",
     EBIBLE.format(id="eng-asv"),
     "American revision of the RV, uses Jehovah; ancestor of the NASB",
     "vpl", "eng-asv"),
    ("YLT", "Young's Literal Translation", "1898", "Public Domain",
     EBIBLE.format(id="engylt"),
     "Robert Young's strictly literal translation (3rd ed. 1898)",
     "vpl", "engylt"),
    ("BSB", "Berean Standard Bible", "2022", "Public Domain (dedicated 2023-04-30)",
     "https://bereanbible.com/bsb.txt",
     "Modern English; publisher's own plain text download",
     "bsb", "https://bereanbible.com/bsb.txt"),
    ("LSV", "Literal Standard Version", "2020", "CC BY-SA 4.0",
     EBIBLE.format(id="englsv"),
     "Modern literal translation by Covenant Press; supplied-word brackets and line marks removed",
     "vpl", "englsv"),
    ("DBY", "Darby Translation", "1890", "Public Domain",
     EBIBLE.format(id="engDBY"),
     "J. N. Darby's New Translation (1890 English Bible)",
     "vpl", "engDBY"),
]

# Characters that must never survive cleaning.
LEFTOVER_RE = re.compile(r"[<>{}\[\]|*¶§†‡\t\n\r\\]|[A-Za-z]\d|\d[A-Za-z]|\b[HG]\d{1,5}\b")


def clean(text):
    """Strip markup, keep normal punctuation, collapse whitespace."""
    t = html.unescape(text)
    t = re.sub(r"<[^>]*>", " ", t)                     # HTML / XML tags
    t = re.sub(r"\{[HG]?\d+[a-z]?\}", " ", t)          # {H1234} Strong's
    t = re.sub(r"\[[HG]\d+[a-z]?\]", " ", t)           # [H1234] Strong's
    t = re.sub(r"(?<=[A-Za-z])[HG]\d{1,5}\b", " ", t)  # glued Strong's
    t = t.replace("||", " ")                           # LSV poetic line marks
    t = t.replace("¶", " ")                            # pilcrows
    t = re.sub(r"[\[\]{}|*†‡]", "", t)                 # supplied-word brackets, note markers
    t = re.sub(r"\s+", " ", t)                          # incl. NBSP / tabs / newlines
    return t.strip()


def load_books():
    """The 66-book canon list (names, abbreviations, USFM ids) kept in data/books.tsv."""
    books = []
    with open(os.path.join(DATA, "books.tsv"), encoding="utf-8") as f:
        for line in f:
            i, name, abbr, usfm, testament, chapters = line.rstrip("\n").split("\t")
            books.append({"name": name, "abbr": abbr, "usfm": usfm, "testament": testament, "chapters": int(chapters)})
    assert len(books) == 66, len(books)
    return books


def fetch(url, cache, name=None):
    os.makedirs(cache, exist_ok=True)
    path = os.path.join(cache, name or os.path.basename(url))
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        print(f"  downloading {url}")
        req = urllib.request.Request(url, headers={"User-Agent": "omabiblia-build/1.0"})
        with urllib.request.urlopen(req, timeout=120) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.replace(path + ".part", path)
    else:
        print(f"  cached {path}")
    return path


def read_kjv(books, cache):
    """aruljohn/Bible-kjv: one JSON per book, {"chapters": [{"chapter", "verses": [{"verse", "text"}]}]}."""
    out = []
    for bi, b in enumerate(books, 1):
        fname = b["name"].replace(" ", "") + ".json"
        with open(fetch(KJV_RAW + fname, os.path.join(cache, "kjv")), encoding="utf-8") as f:
            j = json.load(f)
        for ch in sorted(j["chapters"], key=lambda c: int(c["chapter"])):
            for v in sorted(ch["verses"], key=lambda v: int(v["verse"])):
                t = re.sub(r"<[^>]+>", "", v["text"]).replace("\u2019", "'")
                out.append((bi, int(ch["chapter"]), int(v["verse"]), t))
    return out


def read_vpl(eid, cache):
    zpath = fetch(EBIBLE.format(id=eid), cache)
    code_to_idx = {c: i for i, c in enumerate(VPL_CODES, 1)}
    out, skipped = [], set()
    line_re = re.compile(r"^(\S+) (\d+):(\d+) ?(.*)$")
    with zipfile.ZipFile(zpath) as z:
        with z.open(f"{eid}_vpl.txt") as f:
            for raw in f:
                line = raw.decode("utf-8-sig").rstrip("\r\n")
                if not line.strip():
                    continue
                m = line_re.match(line)
                if not m:
                    raise ValueError(f"{eid}: bad line {line[:80]!r}")
                code, c, v, text = m.groups()
                if code not in code_to_idx:
                    skipped.add(code)
                    continue
                out.append((code_to_idx[code], int(c), int(v), text))
    if skipped:
        print(f"  dropped non-canonical books: {' '.join(sorted(skipped))}")
    return out


def read_bsb(url, books, cache):
    path = fetch(url, cache, "bsb.txt")
    name_to_idx = {b["name"]: i for i, b in enumerate(books, 1)}
    ref_re = re.compile(r"^(.+) (\d+):(\d+)$")
    out = []
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            parts = line.rstrip("\r\n").split("\t")
            if len(parts) != 2:
                continue
            m = ref_re.match(parts[0].strip())
            if not m:
                continue  # header / attribution lines
            name = BSB_NAME_ALIASES.get(m.group(1), m.group(1))
            out.append((name_to_idx[name], int(m.group(2)), int(m.group(3)), parts[1]))
    return out


def write_books(books):
    path = os.path.join(DATA, "books.tsv")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for i, b in enumerate(books, 1):
            f.write("\t".join([str(i), b["name"], b["abbr"], b["usfm"], b["testament"],
                               str(b["chapters"])]) + "\n")
    print(f"wrote {path}")


def write_translation(code, rows):
    seen, out = {}, []
    for bi, c, v, text in rows:
        t = clean(text)
        if not t:
            continue
        key = (bi, c, v)
        if key in seen:
            raise ValueError(f"{code}: duplicate verse {key}")
        seen[key] = True
        out.append((bi, c, v, t))
    out.sort(key=lambda r: r[:3])
    path = os.path.join(OUT, f"{code}.tsv")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for bi, c, v, t in out:
            f.write(f"{bi}\t{c}\t{v}\t{t}\n")
    print(f"wrote {path} ({len(out)} verses)")


# shown by the app next to the text whenever that translation is on screen (licence requirement)
ATTRIBUTION = {
    "LSV": "The Holy Bible in English, Literal Standard Version. Copyright \u00a9 2020 Covenant Press and the "
           "Covenant Christian Coalition. CC BY-SA 4.0 (creativecommons.org/licenses/by-sa/4.0). Formatting removed "
           "for this app; Covenant Press does not necessarily endorse these changes.",
}


def write_index():
    path = os.path.join(OUT, "translations.tsv")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for code, name, year, lic, url, note, *_ in TRANSLATIONS:
            f.write("\t".join([code, name, year, lic, url, note, ATTRIBUTION.get(code, "")]) + "\n")
    print(f"wrote {path}")


NOTICE = """# Bible text licensing and attribution

All texts in this directory were converted by `tools/build_translations.py` to a
plain verse-per-line TSV format (`book_index, chapter, verse, text`). Only the 66
books of the Protestant canon are included, and each translation keeps its own
versification. Licences were checked on the publishers' / distributors' own
pages on 2026-10-09.

Changes made during conversion (all texts): markup removed, whitespace
collapsed (non-breaking spaces become plain spaces), empty verses omitted,
non-canonical books dropped. For ASV, YLT, DBY and LSV the square brackets that
mark words supplied by the translator are removed (the words are kept), as the
KJV source already omits its italics. DBY's `*` note markers and LSV's `||`
poetic line marks are removed.

## KJV: King James Version (1611, 1769 Oxford text)

Public domain in most of the world. In the United Kingdom the Authorized
Version is subject to Crown rights administered by Cambridge University Press
(letters patent); this matters only for printed/distributed copies inside the UK.
Source: https://github.com/aruljohn/Bible-kjv (public-domain JSON; markup stripped).

## WEB: World English Bible (2020 stable text)

Source: eBible.org, https://ebible.org/find/details.php?id=eng-web
(`eng-web_vpl.zip`). Licence as stated by the publisher:

> The World English Bible is in the Public Domain. That means that it is not
> copyrighted. However, "World English Bible" is a Trademark of eBible.org.
> [...] All we ask is that if you CHANGE the actual text of the World English
> Bible in any way, you not call the result the World English Bible any more.

The words of the text are unchanged here (only whitespace normalised and the
deuterocanonical books omitted), so it is presented under its own name.

## ASV: American Standard Version (1901)

Public domain. Source: eBible.org, https://ebible.org/find/details.php?id=eng-asv
(`eng-asv_vpl.zip`), "This public domain Bible translation is brought to you
courtesy of eBible.org."

## YLT: Young's Literal Translation (1898)

Public domain. Source: eBible.org, https://ebible.org/find/details.php?id=engylt
(`engylt_vpl.zip`), "This public domain Bible translation is brought to you
courtesy of eBible.org."

## BSB: Berean Standard Bible

Public domain. Source: the publisher's own download, https://bereanbible.com/bsb.txt.
Terms (https://berean.bible/terms.htm): "The Berean Bible and Majority Bible
texts are officially dedicated to the public domain as of April 30, 2023. All
uses are freely permitted." Attribution is appreciated but not required:

> The Holy Bible, Berean Standard Bible, BSB is produced in cooperation with
> Bible Hub, Discovery Bible, OpenBible.com, and the Berean Bible Translation
> Committee. This text of God's Word has been dedicated to the public domain.

## LSV: Literal Standard Version (2020), CC BY-SA 4.0

**Required attribution (shown with the text in the app):**

> The Holy Bible in English, Literal Standard Version. Copyright © 2020
> Covenant Press and the Covenant Christian Coalition. Licensed under the
> Creative Commons Attribution-ShareAlike 4.0 International licence
> (https://creativecommons.org/licenses/by-sa/4.0/). Source: lsvbible.com via
> eBible.org (https://ebible.org/find/details.php?id=englsv).
> Modified: formatting removed for this app (supplied-word brackets and
> poetic line marks stripped, whitespace normalised); Covenant Press does not
> necessarily endorse these changes.

Publisher's terms (from the eBible.org distribution, `englsv_about.htm`):
"This translation is made available to you under the terms of the Creative
Commons Attribution Share-Alike license 4.0. You have permission to share and
redistribute this Bible translation in any format and to make reasonable
revisions and adaptations of this translation, provided that: You include the
above copyright and source information. If you make any changes to the text,
you must indicate that you did so in a way that makes it clear that the
original licensor is not necessarily endorsing your changes. If you
redistribute this text, you must distribute your contributions under the same
license as the original."

ShareAlike applies to the text file `LSV.tsv` (an adaptation of the text); it
must be redistributed under CC BY-SA 4.0. It does not extend to the app code.

## DBY: Darby Translation (1890)

Public domain. Source: eBible.org, https://ebible.org/find/details.php?id=engDBY
(`engDBY_vpl.zip`).
"""


def write_notice():
    path = os.path.join(OUT, "NOTICE.md")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(NOTICE)
    print(f"wrote {path}")


# ---------------------------------------------------------------- verify
def verify():
    ok = True
    books = []
    with open(os.path.join(DATA, "books.tsv"), encoding="utf-8") as f:
        for line in f:
            p = line.rstrip("\n").split("\t")
            assert len(p) == 6, p
            books.append((int(p[0]), p[1], p[2], p[3], p[4], int(p[5])))
    assert len(books) == 66 and [b[0] for b in books] == list(range(1, 67))
    bk_chapters = {b[0]: b[5] for b in books}
    print(f"books.tsv: {len(books)} books, {sum(bk_chapters.values())} chapters")

    kjv_refs = None
    spots = [(43, 3, 16, "John 3:16"), (1, 1, 1, "Genesis 1:1"), (19, 23, 1, "Psalm 23:1")]
    codes = [t[0] for t in TRANSLATIONS]
    with open(os.path.join(OUT, "translations.tsv"), encoding="utf-8") as f:
        idx_codes = [l.split("\t")[0] for l in f if l.strip()]
    if idx_codes != codes:
        print(f"translations.tsv codes mismatch: {idx_codes}")
        ok = False
    for code in codes:
        path = os.path.join(OUT, f"{code}.tsv")
        refs, verses, bad, prev = [], {}, [], None
        with open(path, encoding="utf-8") as f:
            for n, line in enumerate(f, 1):
                assert line.endswith("\n")
                p = line[:-1].split("\t")
                if len(p) != 4 or not p[3] or p[3] != p[3].strip() or "  " in p[3]:
                    print(f"  {code}:{n}: malformed line"); ok = False; continue
                key = (int(p[0]), int(p[1]), int(p[2]))
                if not (1 <= key[0] <= 66 and key[1] >= 1 and key[2] >= 1):
                    print(f"  {code}:{n}: ref out of range {key}"); ok = False
                if prev is not None and key <= prev:
                    print(f"  {code}:{n}: not sorted/duplicate {key}"); ok = False
                prev = key
                refs.append(key)
                verses[key] = p[3]
                if LEFTOVER_RE.search(p[3]):
                    bad.append((key, p[3]))
        chapters = {}
        for b, c, v in refs:
            chapters.setdefault(b, set()).add(c)
        nchap = sum(len(s) for s in chapters.values())
        print(f"\n{code}: {len(refs)} verses, {len(chapters)} books, {nchap} chapters, "
              f"{os.path.getsize(path)} bytes, leftover-markup verses: {len(bad)}")
        for key, t in bad[:5]:
            print(f"    markup? {key}: {t[:100]}")
        if bad:
            ok = False
        if len(chapters) != 66:
            print(f"  missing books: {sorted(set(range(1, 67)) - set(chapters))}"); ok = False
        for b in range(1, 67):
            have = chapters.get(b, set())
            if have != set(range(1, bk_chapters[b] + 1)):
                print(f"  chapter mismatch {books[b-1][1]}: books.tsv {bk_chapters[b]}, "
                      f"has {len(have)} (max {max(have) if have else 0})")
        if code == "KJV":
            kjv_refs = set(refs)
            if len(refs) != 31102 or nchap != 1189:
                print("  KJV must be 31102 verses / 1189 chapters"); ok = False
        elif kjv_refs is not None:
            s = set(refs)
            extra, missing = sorted(s - kjv_refs), sorted(kjv_refs - s)
            fmt = lambda ks: ", ".join(f"{books[b-1][2]} {c}:{v}" for b, c, v in ks[:25]) + \
                (" ..." if len(ks) > 25 else "")
            print(f"  vs KJV refs: {len(extra)} extra, {len(missing)} missing")
            if extra:
                print(f"    extra: {fmt(extra)}")
            if missing:
                print(f"    missing: {fmt(missing)}")
        for b, c, v, label in spots:
            print(f"  {label}: {verses.get((b, c, v), '<MISSING>')}")
    print("\nVERIFY", "OK" if ok else "FAILED")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--cache", default=DEFAULT_CACHE)
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()
    if not a.verify_only:
        os.makedirs(OUT, exist_ok=True)
        books = load_books()
        for code, name, year, lic, url, note, kind, src in TRANSLATIONS:
            print(f"{code}: {name}")
            if kind == "kjv":
                rows = read_kjv(books, a.cache)
            elif kind == "vpl":
                rows = read_vpl(src, a.cache)
            else:
                rows = read_bsb(src, books, a.cache)
            write_translation(code, rows)
        write_index()
        write_notice()
    sys.exit(0 if verify() else 1)


if __name__ == "__main__":
    main()
