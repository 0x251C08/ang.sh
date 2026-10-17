// expand.cpp
#include "expand.h"
#include "util.h"
#include <cctype>
#include <glob.h>
#include <unistd.h>

namespace ang {

static std::string var_value(const std::string& name, ShellState& st) {
    if (name == "?") return std::to_string(st.last_status);
    if (name == "$") return std::to_string(getpid());
    if (name == "!") return std::to_string(st.last_bg_pid);
    auto it = st.vars.find(name);
    if (it != st.vars.end()) return it->second;
    const char* e = getenv(name.c_str());
    return e ? e : "";
}

std::string expand_one(const std::string& word, ShellState& st, bool quoted) {
    (void)quoted;
    std::string in = word;
    // ~ expansion (unquoted leading ~ only; lexer marks quoted words)
    std::string out;
    for (size_t i = 0; i < in.size();) {
        char c = in[i];
        if (c == '~' && i == 0 && (in.size() == 1 || in[1] == '/')) {
            out += home_dir();
            ++i;
            continue;
        }
        if (c == '$' && i + 1 < in.size()) {
            if (in[i + 1] == '{') {
                size_t e = in.find('}', i + 2);
                if (e == std::string::npos) { out += c; ++i; continue; }
                out += var_value(in.substr(i + 2, e - (i + 2)), st);
                i = e + 1;
                continue;
            }
            if (in[i + 1] == '?' || in[i + 1] == '$' || in[i + 1] == '!') {
                out += var_value(in.substr(i + 1, 1), st);
                i += 2;
                continue;
            }
            if (isalpha((unsigned char)in[i + 1]) || in[i + 1] == '_') {
                size_t j = i + 1;
                while (j < in.size() && (isalnum((unsigned char)in[j]) || in[j] == '_')) ++j;
                out += var_value(in.substr(i + 1, j - (i + 1)), st);
                i = j;
                continue;
            }
            out += c; ++i; continue;
        }
        out += c; ++i;
    }
    return out;
}

std::vector<std::string> expand_args(const std::vector<std::string>& words,
                                     const std::vector<bool>& quoted,
                                     ShellState& st) {
    std::vector<std::string> out;
    for (size_t k = 0; k < words.size(); ++k) {
        std::string e = expand_one(words[k], st, quoted[k]);
        bool has_glob = !quoted[k] &&
            (e.find('*') != std::string::npos ||
             e.find('?') != std::string::npos ||
             e.find('[') != std::string::npos);
        if (has_glob) {
            glob_t g{};
            if (glob(e.c_str(), GLOB_NOCHECK | GLOB_TILDE, nullptr, &g) == 0) {
                for (size_t j = 0; j < g.gl_pathc; ++j) out.push_back(g.gl_pathv[j]);
                globfree(&g);
                continue;
            }
            globfree(&g);
        }
        out.push_back(e);
    }
    return out;
}

}  // namespace ang
