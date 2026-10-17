// highlight.cpp
#include "highlight.h"
#include "lexer.h"
#include "util.h"
#include <cstdlib>
#include <unistd.h>

namespace ang {

static bool on_path(const std::string& cmd) {
    if (cmd.find('/') != std::string::npos) return access(cmd.c_str(), X_OK) == 0;
    const char* p = getenv("PATH");
    std::string path = p ? p : "/bin:/usr/bin";
    size_t a = 0;
    while (a <= path.size()) {
        size_t b = path.find(':', a);
        std::string d = path.substr(a, b == std::string::npos ? b : b - a);
        if (!d.empty() && access((d + "/" + cmd).c_str(), X_OK) == 0) return true;
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return false;
}

static bool is_builtin_name(const std::string& c) {
    for (auto b : {"cd","exit","logout","export","unset","set","alias","unalias",
                   "abbr","posix","source",".","history","help","version","true",
                   "false","type","which","theme","env","echo","pwd","autocd"})
        if (c == b) return true;
    return false;
}

std::string highlight_line(const std::string& line, const ShellState& st) {
    std::string t = trim(line);
    if (!t.empty() && t[0] == '#') return "\033[2m" + line + "\033[0m";
    auto toks = lex(line, nullptr);

    // Re-emit tokens with colors. Spacing is approximated with single gaps
    // (display-only; the real buffer is untouched).
    std::string out;
    bool first_word = true;
    for (size_t k = 0; k < toks.size(); ++k) {
        if (k) out += " ";
        const auto& tk = toks[k];
        if (tk.kind == TokKind::Word) {
            // alias/abbr resolve for color purposes
            std::string w = tk.text;
            auto al = st.aliases.find(w);
            if (first_word && al != st.aliases.end()) w = split_words(al->second).front();
            auto ab = st.abbrs.find(tk.text);
            if (first_word && ab != st.abbrs.end()) w = split_words(ab->second).front();
            if (first_word && (tk.text.find('=') == std::string::npos)) {
                bool ok = is_builtin_name(w) || on_path(w);
                out += ok ? "\033[1;32m" + tk.text + "\033[0m"
                          : "\033[1;31m" + tk.text + "\033[0m";
                first_word = false;
            } else if (tk.quoted) {
                out += "\033[33m" + tk.text + "\033[0m";
            } else if (!tk.text.empty() && tk.text[0] == '-') {
                out += "\033[96m" + tk.text + "\033[0m";
            } else if (!tk.text.empty() && tk.text[0] == '$') {
                out += "\033[35m" + tk.text + "\033[0m";
            } else {
                out += tk.text;
            }
        } else {
            out += "\033[36m" + tk.text + "\033[0m";
            if (tk.kind == TokKind::Semi || tk.kind == TokKind::And ||
                tk.kind == TokKind::Or || tk.kind == TokKind::Bg)
                first_word = true;
        }
        if (tk.kind == TokKind::Pipe) first_word = true;
    }
    return out;
}

std::string autosuggest(const std::string& line, const ShellState& st) {
    if (line.empty()) return "";
    for (auto it = st.history.rbegin(); it != st.history.rend(); ++it) {
        if (it->size() > line.size() && it->compare(0, line.size(), line) == 0)
            return it->substr(line.size());
    }
    return "";
}

}  // namespace ang
