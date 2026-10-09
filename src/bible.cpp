// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "bible.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    size_t a = 0;
    while (true) {
        size_t b = line.find('\t', a);
        out.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    if (!out.empty() && !out.back().empty() && out.back().back() == '\r') out.back().pop_back();
    return out;
}

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool Bible::open(const std::string& dataDir, std::string& err) {
    dir_ = dataDir;
    std::ifstream b(dataDir + "/books.tsv");
    if (!b) { err = "missing " + dataDir + "/books.tsv"; return false; }
    std::string line;
    while (std::getline(b, line)) {
        auto f = splitTabs(line);
        if (f.size() < 6) continue;
        Book k;
        k.index = std::atoi(f[0].c_str()); k.name = f[1]; k.abbr = f[2]; k.usfm = f[3]; k.testament = f[4];
        k.chapters = std::atoi(f[5].c_str());
        books_.push_back(k);
    }
    std::ifstream t(dataDir + "/translations/translations.tsv");
    while (std::getline(t, line)) {
        auto f = splitTabs(line);
        if (f.size() < 2 || f[0].empty() || f[0][0] == '#') continue;
        Translation x;
        x.code = f[0]; x.name = f[1];
        if (f.size() > 2) x.year = f[2];
        if (f.size() > 3) x.license = f[3];
        if (f.size() > 4) x.source = f[4];
        if (f.size() > 5) x.note = f[5];
        if (f.size() > 6) x.attribution = f[6];
        tr_.push_back(x);
    }
    // KJV first if present
    std::stable_sort(tr_.begin(), tr_.end(), [](const Translation& a, const Translation& b) { return (a.code == "KJV") > (b.code == "KJV"); });
    if (books_.size() != 66 || tr_.empty()) { err = "Bible data incomplete in " + dataDir; return false; }
    for (size_t i = 0; i < tr_.size(); i++)
        if (select((int)i)) return true;
    err = "no translation could be loaded";
    return false;
}

bool Bible::load(Translation& t) {
    if (t.loaded) return true;
    std::ifstream f(dir_ + "/translations/" + t.code + ".tsv");
    if (!f) { std::fprintf(stderr, "omabiblia: cannot open %s.tsv\n", t.code.c_str()); return false; }
    t.text.assign(66, {});
    std::string line;
    int n = 0;
    while (std::getline(f, line)) {
        size_t a = line.find('\t'), b = line.find('\t', a + 1), c = line.find('\t', b + 1);
        if (c == std::string::npos) continue;
        int bk = std::atoi(line.c_str()) - 1, ch = std::atoi(line.c_str() + a + 1) - 1, vs = std::atoi(line.c_str() + b + 1) - 1;
        if (bk < 0 || bk >= 66 || ch < 0 || vs < 0 || ch > 200 || vs > 200) continue;
        auto& book = t.text[bk];
        if ((int)book.size() <= ch) book.resize(ch + 1);
        auto& chap = book[ch];
        if ((int)chap.size() <= vs) chap.resize(vs + 1);
        std::string txt = line.substr(c + 1);
        if (!txt.empty() && txt.back() == '\r') txt.pop_back();
        chap[vs] = std::move(txt);
        n++;
    }
    t.totalVerses = n;
    t.loaded = n > 0;
    std::fprintf(stderr, "omabiblia: loaded %s (%d verses)\n", t.code.c_str(), n);
    return t.loaded;
}

bool Bible::select(int i) {
    if (i < 0 || i >= (int)tr_.size()) return false;
    if (!load(tr_[i])) return false;
    cur_ = i;
    return true;
}

int Bible::chapterCount(int book) const {
    if (book < 0 || book >= 66) return 0;
    const auto& t = tr_[cur_];
    return t.loaded && !t.text[book].empty() ? (int)t.text[book].size() : books_[book].chapters;
}
int Bible::verseCount(int book, int chapter) const {
    const auto& t = tr_[cur_];
    if (!t.loaded || book < 0 || book >= 66 || chapter < 1 || chapter > (int)t.text[book].size()) return 0;
    return (int)t.text[book][chapter - 1].size();
}
const std::string& Bible::verse(const Ref& r) const {
    static const std::string empty;
    const auto& t = tr_[cur_];
    if (!t.loaded || r.book < 0 || r.book >= 66 || r.chapter < 1 || r.chapter > (int)t.text[r.book].size()) return empty;
    const auto& c = t.text[r.book][r.chapter - 1];
    return r.verse >= 1 && r.verse <= (int)c.size() ? c[r.verse - 1] : empty;
}
std::string Bible::refString(const Ref& r, bool withVerse) const {
    if (r.book < 0 || r.book >= (int)books_.size()) return "?";
    std::string s = books_[r.book].name + " " + std::to_string(r.chapter);
    if (withVerse) s += ":" + std::to_string(r.verse);
    return s;
}

bool Bible::parseRef(const std::string& in, Ref& out) const {
    std::string s = lower(in);
    // split trailing "c[:v]" from the book part
    size_t p = s.find_last_of("abcdefghijklmnopqrstuvwxyz");
    if (p == std::string::npos) return false;
    std::string bookPart = s.substr(0, p + 1), rest = s.substr(p + 1);
    std::string key;
    for (char c : bookPart) if (std::isalnum((unsigned char)c)) key += c;
    if (key.empty()) return false;
    int best = -1;
    for (int i = 0; i < 66 && best < 0; i++) {
        std::string n, a;
        for (char c : lower(books_[i].name)) if (std::isalnum((unsigned char)c)) n += c;
        for (char c : lower(books_[i].abbr)) if (std::isalnum((unsigned char)c)) a += c;
        if (key == n || key == a || key == lower(books_[i].usfm)) best = i;
    }
    for (int i = 0; i < 66 && best < 0; i++) {        // unique-enough prefix ("gen", "1cor", "ps", "rev")
        std::string n;
        for (char c : lower(books_[i].name)) if (std::isalnum((unsigned char)c)) n += c;
        if (n.compare(0, key.size(), key) == 0) best = i;
    }
    if (best < 0 && (key == "psalm" || key == "ps")) best = 18;
    if (best < 0) return false;
    out = Ref{best, 1, 1};
    int ch = 0, vs = 0;
    if (std::sscanf(rest.c_str(), " %d : %d", &ch, &vs) >= 1 || std::sscanf(rest.c_str(), " %d . %d", &ch, &vs) >= 1) {
        out.chapter = std::max(1, std::min(ch, chapterCount(best)));
        out.verse = vs > 0 ? std::max(1, std::min(vs, std::max(1, verseCount(best, out.chapter)))) : 1;
    }
    return true;
}

std::vector<Hit> Bible::search(const std::string& q, int limit) const {
    std::vector<Hit> out;
    std::string needle = lower(q);
    if (needle.size() < 2) return out;
    const auto& t = tr_[cur_];
    for (int b = 0; b < 66 && (int)out.size() < limit; b++)
        for (int c = 0; c < (int)t.text[b].size() && (int)out.size() < limit; c++)
            for (int v = 0; v < (int)t.text[b][c].size() && (int)out.size() < limit; v++)
                if (lower(t.text[b][c][v]).find(needle) != std::string::npos)
                    out.push_back({Ref{b, c + 1, v + 1}, t.text[b][c][v]});
    return out;
}

// ---- same hashing / PRNG as the parchment app (verse of the day matches it) ----
uint64_t cyrb53(const std::string& str, uint32_t seed) {
    uint32_t h1 = 0xdeadbeef ^ seed, h2 = 0x41c6ce57 ^ seed;
    for (unsigned char ch : str) {      // the JS version hashes UTF-16 units; dates/scope keys are ASCII
        h1 = (h1 ^ ch) * 2654435761u;
        h2 = (h2 ^ ch) * 1597334677u;
    }
    h1 = ((h1 ^ (h1 >> 16)) * 2246822507u) ^ ((h2 ^ (h2 >> 13)) * 3266489909u);
    h2 = ((h2 ^ (h2 >> 16)) * 2246822507u) ^ ((h1 ^ (h1 >> 13)) * 3266489909u);
    return 4294967296ull * (2097151 & h2) + h1;
}
double mulberry32First(uint32_t seed) {
    seed += 0x6D2B79F5u;
    uint32_t t = seed;
    t = (t ^ (t >> 15)) * (1 | t);
    t = (t + ((t ^ (t >> 7)) * (61 | t))) ^ t;
    return ((t ^ (t >> 14)) >> 0) / 4294967296.0;
}
std::string todayIso() {
    std::time_t now = std::time(nullptr);
    std::tm lt{};
    localtime_r(&now, &lt);
    char b[16];
    std::strftime(b, sizeof b, "%Y-%m-%d", &lt);
    return b;
}

static Ref globalToRef(const Bible& bb, const Translation& t, long g) {
    for (int b = 0; b < 66; b++)
        for (int c = 0; c < (int)t.text[b].size(); c++) {
            long n = (long)t.text[b][c].size();
            if (g < n) return Ref{b, c + 1, (int)g + 1};
            g -= n;
        }
    return Ref{};
}

Ref Bible::verseOfDay(const std::string& iso) const {
    // JS: seed = cyrb53(date + "KJV") -> mulberry32 seeded with that number (truncated to int32 by |0)
    uint64_t h = cyrb53(iso + "KJV");
    double r = mulberry32First((uint32_t)(h & 0xffffffffu));
    const auto& t = tr_[cur_];
    return globalToRef(*this, t, (long)(r * t.totalVerses));
}
Ref Bible::randomVerse(unsigned seed) const {
    const auto& t = tr_[cur_];
    double r = mulberry32First(seed);
    return globalToRef(*this, t, (long)(r * t.totalVerses));
}

Ref Bible::step(const Ref& r, int dv) const {
    Ref o = r;
    o.verse += dv;
    if (o.verse < 1) {
        o = stepChapter(o, -1);
        o.verse = std::max(1, verseCount(o.book, o.chapter));
    } else if (o.verse > verseCount(o.book, o.chapter)) {
        o = stepChapter(o, 1);
        o.verse = 1;
    }
    return o;
}
Ref Bible::stepChapter(const Ref& r, int dc) const {
    Ref o = r;
    o.chapter += dc;
    o.verse = 1;
    if (o.chapter < 1) { o.book = (o.book + 65) % 66; o.chapter = std::max(1, chapterCount(o.book)); }
    else if (o.chapter > chapterCount(o.book)) { o.book = (o.book + 1) % 66; o.chapter = 1; }
    return o;
}
