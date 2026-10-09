// SPDX-License-Identifier: Apache-2.0
// bibsh tests: runs commands against the real data and checks what the terminal would show.
#include <cstdio>
#include <string>
#include "bible.h"
#include "shell.h"

static int fails = 0;
static void check(bool ok, const std::string& what, const std::string& got = "") {
    std::printf("%s %s%s\n", ok ? "PASS" : "FAIL", what.c_str(), ok || got.empty() ? "" : ("  -> " + got).c_str());
    if (!ok) fails++;
}

int main(int argc, char** argv) {
    Bible b;
    std::string err;
    if (!b.open(argc > 1 ? argv[1] : "data", err)) { std::printf("open: %s\n", err.c_str()); return 2; }
    BibleShell sh(b);
    Ref opened{-1, 0, 0};
    bool openedChapter = false;
    // the app's hooks, minimal: opening a reading shows it in the scrollback like the app does
    sh.open = [&](Ref r, bool ch) { opened = r; openedChapter = ch; sh.showReading(r, ch); };
    auto last = [&]() { return sh.lines.empty() ? std::string() : sh.lines.back().s; };
    auto has = [&](const std::string& needle, size_t from) {
        for (size_t i = from; i < sh.lines.size(); i++) if (sh.lines[i].s.find(needle) != std::string::npos) return true;
        return false;
    };
    auto run = [&](const std::string& c) { size_t n = sh.lines.size(); sh.exec(c); return n; };

    size_t n = run("cat jer/29:11");
    check(opened.book == 23 && opened.chapter == 29 && opened.verse == 1 + 10 && !openedChapter, "cat jer/29:11 opens Jeremiah 29:11");
    check(has("For I know the thoughts that I think toward you", n), "cat jer/29:11 prints the verse", last());
    check(sh.lines[n].s.find("$ cat jer/29:11") != std::string::npos, "command echoed once, before the text", sh.lines[n].s);
    check(!has("$ cat jer/29:11", n + 1), "no duplicate echo of the cat line");

    n = run("cat jer/39:17");
    check(has("But I will deliver thee in that day", n), "cat jer/39:17 (the screenshot)");
    n = run("cat jer/29:11-13");
    check(has("11 For I know", n) && has("12 Then shall ye call upon me", n) && has("13 And ye shall seek me", n), "range jer/29:11-13 prints 3 verses in order");
    n = run("cat jer/29");
    check(openedChapter && opened.chapter == 29 && has("14 And I will be found of you", n), "cat jer/29 opens the whole chapter");
    n = run("cat jeremiah 29:11");     check(has("thoughts that I think", n), "cat jeremiah 29:11");
    n = run("cat /JER/29:11");          check(has("thoughts that I think", n), "absolute, upper case");
    n = run("cat jer.29.11");           check(has("thoughts that I think", n), "dotted jer.29.11");
    n = run("cat 1cor/13:4");           check(has("Charity suffereth long", n), "cat 1cor/13:4");
    n = run("cat 1 cor 13:4");          check(has("Charity suffereth long", n), "cat 1 cor 13:4");
    n = run("john 3:16");               check(has("For God so loved the world", n), "bare reference reads");
    n = run("cd ps");                   check(sh.cwd() == "/psa", "cd ps -> /psa", sh.cwd());
    n = run("cat 23:1");                check(has("The LORD is my shepherd", n), "relative cat 23:1 inside /psa");
    n = run("cd 23");                   check(sh.cwd() == "/psa/23", "cd 23 -> /psa/23", sh.cwd());
    n = run("cat 1-2");                 check(has("my shepherd", n) && has("green pastures", n), "relative verse range inside a chapter");
    n = run("pwd");                     check(last() == "/psa/23", "pwd", last());
    n = run("cd ..");                   check(sh.cwd() == "/psa", "cd ..", sh.cwd());
    n = run("cd /");                    check(sh.cwd() == "/", "cd /", sh.cwd());
    n = run("ls");                      check(has("gen/", n) && has("rev/", n), "ls at / lists books");
    n = run("ls jer");                  check(has("52", n), "ls jer lists 52 chapters");
    n = run("ls -l");                   check(has("Jeremiah", n) && has("52 ch", n), "ls -l");
    n = run("grep \"the lord is my shepherd\"");  check(has("psa/23:1", n), "grep phrase across the Bible", last());
    n = run("cat prov/3 | grep heart"); check(has("prov/3:5", n) == false && has("pro/3:5", n), "pipe cat | grep (paths use usfm ids)", last());
    n = run("grep love | wc -l");       check(std::atoi(last().c_str()) > 200, "grep | wc -l", last());
    n = run("grep love | head -n 3");   check(sh.lines.size() - n == 4, "grep | head -n 3 -> 3 lines", std::to_string(sh.lines.size() - n));
    n = run("cat ps/119 | wc -w");      check(std::atoi(last().c_str()) > 2000, "cat ps/119 | wc -w", last());
    n = run("cat jer/99:1");            check(sh.lines.back().kind == TLine::Err, "bad chapter is an error", last());
    n = run("cat zzz/1:1");             check(sh.lines.back().kind == TLine::Err, "bad book is an error", last());
    n = run("rm -rf /");                check(sh.lines.back().kind == TLine::Err && last().find("command not found") != std::string::npos, "no system commands", last());
    n = run("cat");                     check(sh.lines.back().kind == TLine::Err, "cat without a path explains itself");
    sh.input = "ca"; sh.complete();     check(sh.input == "cat ", "Tab completes a command", sh.input);
    sh.input = "cat jer"; sh.complete(); check(sh.input == "cat jer/", "Tab completes a book", sh.input);
    sh.input = ""; sh.historyUp();      check(sh.input == "cat", "history: Up recalls the last command run", sh.input);
    sh.historyUp();                     check(sh.input == "rm -rf /", "history: Up again", sh.input);
    sh.historyDown(); sh.historyDown(); check(sh.input.empty(), "history: Down back to the empty line", sh.input);
    n = run("history");                 check(has("cat jer/29:11", n), "history lists commands");
    sh.clear();                         check(sh.lines.empty(), "clear");
    std::printf("%s (%d failed)\n", fails ? "FAILED" : "ALL PASS", fails);
    return fails ? 1 : 0;
}
