// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - offline Bible store (public-domain / freely licensed texts in data/).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Book {
    int index = 0;                 // 1..66
    std::string name, abbr, usfm, testament;
    int chapters = 0;
};

struct Translation {
    std::string code, name, year, license, source, note, attribution;   // attribution: must be shown with the text (CC)
    bool loaded = false;
    std::vector<std::vector<std::vector<std::string>>> text;   // [book 0..65][chapter 0..][verse 0..] ("" = absent)
    int totalVerses = 0;
};

struct Ref { int book = 0, chapter = 1, verse = 1; };          // book is 0-based here

struct Hit { Ref ref; std::string text; };

class Bible {
public:
    bool open(const std::string& dataDir, std::string& err);
    const std::vector<Book>& books() const { return books_; }
    std::vector<Translation>& translations() { return tr_; }
    int current() const { return cur_; }
    bool select(int i);                                         // loads on demand
    Translation& tr() { return tr_[cur_]; }

    int chapterCount(int book) const;
    int verseCount(int book, int chapter) const;                // in the current translation
    const std::string& verse(const Ref& r) const;              // "" if absent
    std::string refString(const Ref& r, bool withVerse = true) const;
    bool parseRef(const std::string& s, Ref& out) const;       // "john 3:16", "1 cor 13", "ps 23"
    std::vector<Hit> search(const std::string& q, int limit) const;
    Ref verseOfDay(const std::string& isoDate) const;           // same seed scheme as the parchment app
    Ref randomVerse(unsigned seed) const;
    Ref step(const Ref& r, int dv) const;                       // next / previous verse across chapters/books
    Ref stepChapter(const Ref& r, int dc) const;

private:
    bool load(Translation& t);
    std::string dir_;
    std::vector<Book> books_;
    std::vector<Translation> tr_;
    int cur_ = 0;
};

uint64_t cyrb53(const std::string& s, uint32_t seed = 0);
double mulberry32First(uint32_t seed);
std::string todayIso();
