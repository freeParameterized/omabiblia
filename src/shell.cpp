// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
#include "shell.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace {
std::string lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
std::string alnum(const std::string& s) { std::string o; for (char c : s) if (std::isalnum((unsigned char)c)) o += (char)std::tolower((unsigned char)c); return o; }
bool isNum(const std::string& s) { return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return std::isdigit((unsigned char)c); }); }

std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> t;
    std::string cur;
    bool q = false, any = false;
    char qc = 0;
    for (char c : s) {
        if (q) { if (c == qc) q = false; else cur += c; continue; }
        if (c == '"' || c == '\'') { q = true; qc = c; any = true; continue; }
        if (std::isspace((unsigned char)c)) { if (!cur.empty() || any) t.push_back(cur); cur.clear(); any = false; continue; }
        cur += c;
    }
    if (!cur.empty() || any) t.push_back(cur);
    return t;
}
std::vector<std::string> splitPipes(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    bool q = false;
    char qc = 0;
    for (char c : s) {
        if (q) { if (c == qc) q = false; cur += c; continue; }
        if (c == '"' || c == '\'') { q = true; qc = c; cur += c; continue; }
        if (c == '|') { parts.push_back(cur); cur.clear(); continue; }
        cur += c;
    }
    parts.push_back(cur);
    return parts;
}
const char* kCommands[] = {"cat", "cd", "ls", "pwd", "grep", "head", "tail", "wc", "sort", "uniq", "echo", "clear", "history",
                           "help", "man", "random", "fortune", "today", "votd", "next", "prev", "chapter", "verse", "tr",
                           "scene", "exit", "date", "whoami", "uname", "less", "more", "open", "find"};
}  // namespace

int BibleShell::findBook(const std::string& raw) const {
    std::string key = alnum(raw);
    if (key.empty()) return -1;
    const auto& B = b_.books();
    for (int i = 0; i < (int)B.size(); i++)
        if (key == alnum(B[i].name) || key == alnum(B[i].abbr) || key == alnum(B[i].usfm)) return i;
    if (key == "ps" || key == "psalm") return 18;
    if (key == "song" || key == "songs" || key == "sos") return 21;
    for (int i = 0; i < (int)B.size(); i++)
        if (alnum(B[i].name).compare(0, key.size(), key) == 0) return i;
    return -1;
}

std::string BibleShell::cwd() const {
    if (cwdBook_ < 0) return "/";
    std::string s = "/" + lower(b_.books()[cwdBook_].usfm);
    if (cwdChapter_ > 0) s += "/" + std::to_string(cwdChapter_);
    return s;
}

std::string BibleShell::pathOf(const Span& s) const {
    if (s.book < 0) return "/";
    std::string p = lower(b_.books()[s.book].usfm);
    if (s.chapter > 0) p += "/" + std::to_string(s.chapter);
    if (s.v1 > 0) p += ":" + std::to_string(s.v1) + (s.v2 > s.v1 ? "-" + std::to_string(s.v2) : "");
    return p;
}

// jer/29:11  jer/29:11-14  jer/29  jer  /jer/29  jeremiah 29:11  jer.29.11  29:11 (relative)  ..  /
bool BibleShell::resolve(const std::string& argIn, Span& out) const {
    std::string a = lower(argIn);
    while (!a.empty() && std::isspace((unsigned char)a.back())) a.pop_back();
    out = Span{cwdBook_, cwdChapter_, 0, 0};
    if (a.empty() || a == ".") return true;
    if (a == "/" || a == "~") { out = Span{}; return true; }
    if (a == "..") {
        if (out.chapter > 0) out.chapter = 0; else out.book = -1;
        return true;
    }
    bool absolute = a[0] == '/';
    while (!a.empty() && a[0] == '/') a.erase(a.begin());
    // '/' and '.' separate parts; ':' and '-' stay meaningful
    for (auto& c : a) if (c == '/' || c == '.') c = ' ';
    std::vector<std::string> parts;
    { std::istringstream ss(a); std::string w; while (ss >> w) parts.push_back(w); }
    if (parts.empty()) { out = Span{}; return true; }
    auto parseCV = [&](const std::string& t, int& ch, int& v1, int& v2) {
        ch = v1 = v2 = 0;
        size_t colon = t.find(':');
        std::string c = t.substr(0, colon);
        if (!isNum(c)) return false;
        ch = std::atoi(c.c_str());
        if (colon != std::string::npos) {
            std::string v = t.substr(colon + 1);
            size_t dash = v.find('-');
            std::string a1 = v.substr(0, dash), a2 = dash == std::string::npos ? "" : v.substr(dash + 1);
            if (!isNum(a1) || (!a2.empty() && !isNum(a2))) return false;
            v1 = std::atoi(a1.c_str());
            v2 = a2.empty() ? v1 : std::atoi(a2.c_str());
        }
        return true;
    };
    // relative: "29:11" / "29" inside a book, "11" / "11-14" inside a chapter
    if (!absolute && parts.size() <= 2 && out.book >= 0 && (std::isdigit((unsigned char)parts[0][0])) &&
        parts[0].find_first_not_of("0123456789:-") == std::string::npos) {
        int ch, v1, v2;
        std::string t = parts[0];
        if (parts.size() == 2) t += ":" + parts[1];
        if (out.chapter > 0 && t.find(':') == std::string::npos) {
            size_t dash = t.find('-');
            std::string a1 = t.substr(0, dash), a2 = dash == std::string::npos ? "" : t.substr(dash + 1);
            if (isNum(a1) && (a2.empty() || isNum(a2))) {
                out.v1 = std::atoi(a1.c_str());
                out.v2 = a2.empty() ? out.v1 : std::atoi(a2.c_str());
                return out.v1 >= 1;
            }
        }
        if (!parseCV(t, ch, v1, v2)) return false;
        out.chapter = ch; out.v1 = v1; out.v2 = v2;
    } else {
        // book name may itself start with a digit ("1 cor", "1cor"); everything up to the first
        // pure chapter token is the book
        size_t i = 0;
        std::string bookKey;
        while (i < parts.size()) {
            const std::string& t = parts[i];
            bool chapterTok = std::isdigit((unsigned char)t[0]) && t.find_first_not_of("0123456789:-") == std::string::npos;
            if (chapterTok && !bookKey.empty()) break;
            bookKey += t;
            i++;
        }
        int bk = findBook(bookKey);
        if (bk < 0) return false;
        out = Span{bk, 0, 0, 0};
        if (i < parts.size()) {
            std::string t = parts[i];
            if (i + 1 < parts.size()) t += ":" + parts[i + 1];       // "jer 29 11" / jer.29.11
            int ch, v1, v2;
            if (!parseCV(t, ch, v1, v2)) return false;
            out.chapter = ch; out.v1 = v1; out.v2 = v2;
        }
    }
    if (out.chapter < 0 || out.chapter > b_.chapterCount(out.book)) return false;
    if (out.chapter > 0) {
        int n = b_.verseCount(out.book, out.chapter);
        if (out.v1 > n) return false;
        out.v2 = std::min(out.v2, n);
    }
    return true;
}

void BibleShell::out(const std::string& s, TLine::Kind k) {
    lines.push_back({k, s});
    if (lines.size() > 600) {
        size_t drop = lines.size() - 600;
        lines.erase(lines.begin(), lines.begin() + drop);
        if (typeFrom != (size_t)-1) typeFrom = typeFrom >= drop ? typeFrom - drop : 0;
    }
}

std::vector<std::string> BibleShell::catSpan(const Span& s, bool withPath) const {
    std::vector<std::string> o;
    auto emitChapter = [&](int bk, int ch, int v1, int v2) {
        int n = b_.verseCount(bk, ch);
        for (int v = std::max(1, v1); v <= std::min(n, v2); v++) {
            const std::string& t = b_.verse({bk, ch, v});
            if (t.empty()) continue;
            std::string p = withPath ? lower(b_.books()[bk].usfm) + "/" + std::to_string(ch) + ":" + std::to_string(v) + "  " : std::to_string(v) + " ";
            o.push_back(p + t);
        }
    };
    if (s.book < 0) return o;
    if (s.chapter == 0) {
        for (int c = 1; c <= b_.chapterCount(s.book); c++) emitChapter(s.book, c, 1, 1 << 20);
    } else if (s.v1 == 0) emitChapter(s.book, s.chapter, 1, 1 << 20);
    else emitChapter(s.book, s.chapter, s.v1, s.v2);
    return o;
}

void BibleShell::showReading(const Ref& r, bool chapter) {
    Span s{r.book, r.chapter, chapter ? 0 : r.verse, chapter ? 0 : std::max(r.verse, rangeEnd_)};
    rangeEnd_ = 0;
    if (echoNext) out("$ cat " + pathOf(s), TLine::Cmd);
    echoNext = true;
    typeFrom = lines.size();
    typeSerial++;
    for (auto& l : catSpan(s, false)) out(l, TLine::Text);
}

void BibleShell::exec(const std::string& lineIn) {
    std::string line = lineIn;
    while (!line.empty() && std::isspace((unsigned char)line.back())) line.pop_back();
    out(prompt() + line, TLine::Cmd);
    typeFrom = (size_t)-1;            // older readings stop animating
    if (line.find_first_not_of(" \t") == std::string::npos) return;
    if (hist_.empty() || hist_.back() != line) hist_.push_back(line);
    histPos_ = -1;
    auto stages = splitPipes(line);
    std::vector<std::string> data;
    bool havePipe = stages.size() > 1;
    for (size_t i = 0; i < stages.size(); i++) {
        auto argv = tokenize(stages[i]);
        if (argv.empty()) { out("bibsh: syntax error near '|'", TLine::Err); return; }
        bool handled = false;
        data = run(argv, i ? &data : nullptr, i + 1 == stages.size() && !havePipe, handled);
        if (handled) return;           // command did its own output (or opened a reading)
    }
    const size_t cap = 400;
    for (size_t i = 0; i < data.size() && i < cap; i++) out(data[i]);
    if (data.size() > cap) out("... (" + std::to_string(data.size() - cap) + " more lines; narrow it with grep or head)", TLine::Dim);
}

std::vector<std::string> BibleShell::run(const std::vector<std::string>& argv, const std::vector<std::string>* in, bool alone, bool& handled) {
    const std::string cmd = lower(argv[0]);
    std::vector<std::string> o;
    auto err = [&](const std::string& m) { out(m, TLine::Err); handled = true; return std::vector<std::string>{}; };
    auto flagN = [&](int def) {
        int n = def;
        for (size_t i = 1; i < argv.size(); i++) {
            if (argv[i] == "-n" && i + 1 < argv.size()) n = std::atoi(argv[i + 1].c_str());
            else if (argv[i].size() > 1 && argv[i][0] == '-' && isNum(argv[i].substr(1))) n = std::atoi(argv[i].c_str() + 1);
        }
        return std::max(0, n);
    };
    std::vector<std::string> args;
    for (size_t i = 1; i < argv.size(); i++) args.push_back(argv[i]);

    if (cmd == "help" || cmd == "man" || cmd == "?") {
        const char* h[] = {
            "bibsh - the Bible as a read-only filesystem. paths: jer/29:11  jer/29:11-14  jer/29  jer  /  ..",
            "  ls [-l] [path]        books, chapters or verses        cd <path>   pwd",
            "  cat <path> [...]      read (also: less, more, open)    cat 29:11 inside /jer",
            "  grep [-c] <words> [path]   search (case-insensitive; whole Bible from /)",
            "  ... | grep | head -n N | tail -n N | wc [-l|-w] | sort | uniq",
            "  random  today  next  prev  chapter  verse      tr [code]   scene <1-5>   exit",
            "  history  clear (Ctrl+L)  echo  date  whoami  uname     Tab completes, Up/Down history",
            "  e.g.  cat jer/29:11    cd ps; cat 23    grep \"the lord is my\" | head -5    cat prov/3 | grep heart"};
        for (auto s : h) out(s, TLine::Dim);
        handled = true;
        return o;
    }
    if (cmd == "clear" || cmd == "cls") { clear(); handled = true; return o; }
    if (cmd == "pwd") { o.push_back(cwd()); return o; }
    if (cmd == "whoami") { o.push_back(user); return o; }
    if (cmd == "uname") { o.push_back("bibsh 0.1 (omabiblia) - offline, no network"); return o; }
    if (cmd == "date") {
        std::time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof buf, "%a %b %e %H:%M:%S %Y", std::localtime(&t));
        o.push_back(buf);
        return o;
    }
    if (cmd == "echo") { std::string s; for (auto& a : args) s += (s.empty() ? "" : " ") + a; o.push_back(s); return o; }
    if (cmd == "history") { for (size_t i = 0; i < hist_.size(); i++) o.push_back(std::to_string(i + 1) + "  " + hist_[i]); return o; }
    if (cmd == "random" || cmd == "fortune") { if (random) { echoNext = false; random(); } handled = true; return o; }
    if (cmd == "today" || cmd == "votd") { if (today) { echoNext = false; today(); } handled = true; return o; }
    if (cmd == "next") { if (next) { echoNext = false; next(); } handled = true; return o; }
    if (cmd == "prev") { if (prev) { echoNext = false; prev(); } handled = true; return o; }
    if (cmd == "chapter" || cmd == "verse") { if (chapterMode) { echoNext = false; chapterMode(cmd == "chapter"); } handled = true; return o; }
    if (cmd == "exit" || cmd == "logout") { if (scene) scene(0); handled = true; return o; }
    if (cmd == "scene") {
        int n = args.empty() ? 0 : std::atoi(args[0].c_str());
        if (n < 1 || n > 5) return err("scene: 1 crawl  2 link  3 command  4 hacker  5 retro");
        if (scene) scene(n - 1);
        handled = true;
        return o;
    }
    if (cmd == "tr") {
        if (args.empty()) { o.push_back(translations ? translations() : ""); return o; }
        std::string c = args[0];
        for (auto& ch : c) ch = (char)std::toupper((unsigned char)ch);
        if (!translation || !translation(c)) return err("tr: no translation '" + args[0] + "'. have: " + (translations ? translations() : ""));
        out("-> " + c, TLine::Dim);
        handled = true;
        return o;
    }
    if (cmd == "cd") {
        Span s;
        std::string a = args.empty() ? "/" : args[0];
        if (!resolve(a, s) || s.v1 > 0) return err("cd: " + a + ": no such book or chapter");
        cwdBook_ = s.book; cwdChapter_ = s.chapter;
        handled = true;
        return o;
    }
    if (cmd == "ls" || cmd == "dir" || cmd == "find") {
        bool longFmt = false;
        std::string a;
        for (auto& x : args) { if (x == "-l" || x == "-la" || x == "-al") longFmt = true; else a = x; }
        Span s;
        if (!resolve(a, s)) return err("ls: cannot access '" + a + "': no such book or chapter");
        const auto& B = b_.books();
        if (s.book < 0) {
            if (longFmt) {
                for (int i = 0; i < (int)B.size(); i++) {
                    char buf[160];
                    std::snprintf(buf, sizeof buf, "dr-xr-xr-x  %3d ch  %-4s  %s%s", b_.chapterCount(i), lower(B[i].usfm).c_str(), B[i].name.c_str(), i == 0 ? "   (old testament)" : i == 39 ? "   (new testament)" : "");
                    o.push_back(buf);
                }
            } else {
                std::string row;
                for (int i = 0; i < (int)B.size(); i++) {
                    std::string n = lower(B[i].usfm) + "/";
                    n.resize(6, ' ');
                    row += n;
                    if (i % 11 == 10 || i == (int)B.size() - 1) { o.push_back(row); row.clear(); }
                }
            }
        } else if (s.chapter == 0) {
            int n = b_.chapterCount(s.book);
            if (longFmt) o.push_back(B[s.book].name + ": " + std::to_string(n) + " chapters");
            std::string row;
            for (int c = 1; c <= n; c++) {
                std::string t = std::to_string(c);
                t.resize(4, ' ');
                row += t;
                if (c % 20 == 0 || c == n) { o.push_back(row); row.clear(); }
            }
        } else {
            int n = b_.verseCount(s.book, s.chapter);
            if (longFmt) { for (auto& l : catSpan(s, true)) o.push_back(l.substr(0, 90) + (l.size() > 90 ? "..." : "")); return o; }
            o.push_back(B[s.book].name + " " + std::to_string(s.chapter) + ": " + std::to_string(n) + " verses (cat " + pathOf(s) + ")");
        }
        return o;
    }
    if (cmd == "cat" || cmd == "less" || cmd == "more" || cmd == "open" || cmd == "read") {
        if (args.empty()) {
            if (in) return *in;
            return err(cmd + ": which passage? e.g. cat jer/29:11");
        }
        // "jeremiah 29:11" / "1 cor 13:4" are one passage written with spaces; "jer/29:11 ps/23" are two
        std::vector<Span> spans;
        std::string joined;
        for (auto& a : args) joined += (joined.empty() ? "" : " ") + a;
        Span one;
        if (args.size() > 1 && resolve(joined, one) && one.book >= 0) spans.push_back(one);
        else
            for (auto& a : args) {
                Span s;
                if (!resolve(a, s) || s.book < 0) return err(cmd + ": " + a + ": no such passage");
                spans.push_back(s);
            }
        // a single passage with nothing piped: it becomes the reading (typed out, all scenes follow)
        if (alone && spans.size() == 1 && spans[0].chapter > 0 && open) {
            const Span& s = spans[0];
            echoNext = false;
            rangeEnd_ = s.v2;
            open(Ref{s.book, s.chapter, s.v1 > 0 ? s.v1 : 1}, s.v1 == 0);
            handled = true;
            return o;
        }
        bool withPath = spans.size() > 1 || spans[0].chapter == 0 || !alone;
        for (auto& s : spans) for (auto& l : catSpan(s, withPath)) o.push_back(l);
        return o;
    }
    if (cmd == "grep") {
        bool count = false, invert = false;
        std::vector<std::string> rest;
        for (auto& a : args) {
            if (a == "-c") count = true;
            else if (a == "-v") invert = true;
            else if (a == "-i" || a == "-n" || a == "-F") {}
            else rest.push_back(a);
        }
        if (rest.empty()) return err("grep: what? e.g. grep shepherd   or   cat ps/23 | grep shepherd");
        std::string needle = lower(rest[0]);
        std::vector<std::string> src;
        if (in) src = *in;
        else {
            // remaining words: either more search words or a path as the last argument
            Span scope{cwdBook_, cwdChapter_, 0, 0};
            if (rest.size() > 1) {
                Span s;
                if (resolve(rest.back(), s)) { scope = s; rest.pop_back(); }
                needle.clear();
                for (auto& w : rest) needle += (needle.empty() ? "" : " ") + lower(w);
            }
            if (scope.book < 0) {
                for (int bk = 0; bk < 66; bk++) {
                    auto l = catSpan(Span{bk, 0, 0, 0}, true);
                    for (auto& x : l) if ((lower(x).find(needle) != std::string::npos) != invert) src.push_back(x);
                }
                if (count) { o.push_back(std::to_string(src.size())); return o; }
                return src;
            }
            src = catSpan(scope, true);
        }
        for (auto& x : src) if ((lower(x).find(needle) != std::string::npos) != invert) o.push_back(x);
        if (count) return {std::to_string(o.size())};
        return o;
    }
    if (cmd == "head" || cmd == "tail") {
        if (!in) return err(cmd + ": use it after a pipe, e.g. grep love | " + cmd + " -n 5");
        size_t n = (size_t)flagN(10);
        if (cmd == "head") o.assign(in->begin(), in->begin() + std::min(n, in->size()));
        else o.assign(in->end() - std::min(n, in->size()), in->end());
        return o;
    }
    if (cmd == "wc") {
        if (!in) return err("wc: use it after a pipe, e.g. cat ps/119 | wc -w");
        size_t words = 0, chars = 0;
        for (auto& l : *in) { std::istringstream ss(l); std::string w; while (ss >> w) words++; chars += l.size() + 1; }
        bool l = std::find(args.begin(), args.end(), "-l") != args.end(), w = std::find(args.begin(), args.end(), "-w") != args.end();
        if (l) o.push_back(std::to_string(in->size()));
        else if (w) o.push_back(std::to_string(words));
        else o.push_back(std::to_string(in->size()) + " lines  " + std::to_string(words) + " words  " + std::to_string(chars) + " chars");
        return o;
    }
    if (cmd == "sort") { if (!in) return err("sort: use it after a pipe"); o = *in; std::sort(o.begin(), o.end()); return o; }
    if (cmd == "uniq") { if (!in) return err("uniq: use it after a pipe"); for (auto& x : *in) if (o.empty() || o.back() != x) o.push_back(x); return o; }

    // not a command: maybe a bare passage ("jer 29:11", "ps 23") - read it
    Span s;
    std::string joined;
    for (auto& a : argv) joined += (joined.empty() ? "" : " ") + a;
    if (alone && resolve(joined, s) && s.book >= 0 && open) {
        if (s.chapter == 0) { cwdBook_ = s.book; cwdChapter_ = 0; out("(cd " + cwd() + " - ls to list chapters)", TLine::Dim); handled = true; return o; }
        echoNext = false;
        open(Ref{s.book, s.chapter, s.v1 > 0 ? s.v1 : 1}, s.v1 == 0);
        handled = true;
        return o;
    }
    return err("bibsh: " + argv[0] + ": command not found (help lists commands; cat jer/29:11 reads)");
}

void BibleShell::complete() {
    size_t sp = input.find_last_of(" |");
    std::string head = sp == std::string::npos ? "" : input.substr(0, sp + 1), word = sp == std::string::npos ? input : input.substr(sp + 1);
    bool first = head.find_first_not_of(" |") == std::string::npos && sp == std::string::npos;
    std::vector<std::string> cands;
    std::string lw = lower(word);
    if (first || (!head.empty() && head.back() == '|')) {
        for (auto c : kCommands) if (std::string(c).compare(0, lw.size(), lw) == 0) cands.push_back(std::string(c) + " ");
    } else {
        std::string prefix = lw;
        bool slash = prefix.find('/') != std::string::npos;
        if (!slash) {
            for (auto& b : b_.books()) {
                std::string u = lower(b.usfm);
                if (u.compare(0, prefix.size(), prefix) == 0) cands.push_back(u + "/");
            }
            if (cands.empty())
                for (auto& b : b_.books()) {
                    std::string n = alnum(b.name);
                    if (n.compare(0, prefix.size(), prefix) == 0) cands.push_back(lower(b.usfm) + "/");
                }
        }
    }
    if (cands.empty()) return;
    if (cands.size() == 1) { input = head + cands[0]; return; }
    // common prefix, then list
    std::string common = cands[0];
    for (auto& c : cands) { size_t i = 0; while (i < common.size() && i < c.size() && common[i] == c[i]) i++; common.resize(i); }
    if (common.size() > word.size()) { input = head + common; return; }
    std::string row;
    for (auto& c : cands) row += c + "  ";
    out(prompt() + input, TLine::Cmd);
    out(row, TLine::Dim);
}

void BibleShell::historyUp() {
    if (hist_.empty()) return;
    if (histPos_ < 0) { histSaved_ = input; histPos_ = (int)hist_.size(); }
    if (histPos_ > 0) histPos_--;
    input = hist_[histPos_];
}
void BibleShell::historyDown() {
    if (histPos_ < 0) return;
    if (++histPos_ >= (int)hist_.size()) { histPos_ = -1; input = histSaved_; return; }
    input = hist_[histPos_];
}
void BibleShell::cancel() {
    out(prompt() + input + "^C", TLine::Cmd);
    input.clear();
    histPos_ = -1;
}
