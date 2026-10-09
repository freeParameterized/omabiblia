# Bible text licensing and attribution

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
