// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Free Parameter LLC
// Omabiblia - "bibsh", the Hacker scene's shell. The Bible is a read-only filesystem:
//   /                 the 66 books (gen exo lev ... rev)
//   /jer              chapters of Jeremiah
//   /jer/29           verses of Jeremiah 29
// cat jer/29:11, cat jer/29:11-14, cd jer, ls, grep, pipes (| grep | head | tail | wc | sort | uniq),
// history, tab completion. It is not a system shell: nothing outside the Bible is reachable.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "bible.h"

struct TLine {
    enum Kind { Out, Cmd, Text, Err, Dim } kind = Out;
    std::string s;
};

struct Span { int book = -1, chapter = 0, v1 = 0, v2 = 0; };   // chapter 0 = whole book, v1 0 = whole chapter

class BibleShell {
public:
    // hooks into the app
    std::function<void(Ref, bool chapter)> open;      // show this reading everywhere
    std::function<void()> random, today, next, prev;
    std::function<void(int)> scene;
    std::function<bool(const std::string&)> translation;   // code -> ok
    std::function<std::string()> translations;               // "KJV WEB ..."
    std::function<void(bool)> chapterMode;

    explicit BibleShell(Bible& b) : b_(b) {}
    std::string user = "reader", host = "omarchy";

    std::vector<TLine> lines;
    size_t typeFrom = (size_t)-1;     // lines from here on are typed out (the newest reading)
    int typeSerial = 0;
    std::string input;

    std::string cwd() const;
    std::string prompt() const { return user + "@" + host + ":" + cwd() + "$ "; }
    void exec(const std::string& line);
    void showReading(const Ref& r, bool chapter);       // called when the reading changes
    void complete();                                     // Tab
    void historyUp();
    void historyDown();
    void cancel();                                       // Ctrl+C
    void clear() { lines.clear(); typeFrom = (size_t)-1; }
    bool echoNext = true;                                // false while a typed `cat` already echoed itself

    // exposed for tests
    bool resolve(const std::string& arg, Span& out) const;
    std::string pathOf(const Span& s) const;

private:
    void out(const std::string& s, TLine::Kind k = TLine::Out);
    std::vector<std::string> run(const std::vector<std::string>& argv, const std::vector<std::string>* in, bool last, bool& handled);
    std::vector<std::string> catSpan(const Span& s, bool withPath) const;
    int findBook(const std::string& key) const;
    Bible& b_;
    int cwdBook_ = -1, cwdChapter_ = 0;
    int rangeEnd_ = 0;                 // cat jer/29:11-14: the next reading runs to verse 14
    std::vector<std::string> hist_;
    int histPos_ = -1;
    std::string histSaved_;
};
