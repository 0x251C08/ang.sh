// expand.cpp
#include "expand.h"
#include "executor.h"   // run_jobs
#include "lexer.h"      // lex
#include "parser.h"     // parse, JobList
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <glob.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ang {

namespace {
int g_cs_depth = 0;  // command-substitution recursion guard
}

// Run `inner` as a command (captured stdout) — implements $(...).
// Runs in a forked grandchild so inner state changes never leak out.
static std::string run_captured(const std::string& inner, ShellState& st) {
    if (++g_cs_depth > 8) { --g_cs_depth; return ""; }
    std::string out;
    std::string lerr;
    auto toks = lex(inner, &lerr);
    if (lerr.empty() && !toks.empty()) {
        JobList jobs;
        std::string perr;
        if (parse(toks, st.posix, &jobs, &perr)) {
            int pipefd[2] = {-1, -1};
            if (pipe(pipefd) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                    signal(SIGINT, SIG_DFL);
                    int rc = run_jobs(jobs, st);
                    fflush(nullptr);
                    _exit(rc);
                }
                close(pipefd[1]);
                char buf[4096];
                ssize_t n = 0;
                while ((n = read(pipefd[0], buf, sizeof buf)) > 0)
                    out.append(buf, (size_t)n);
                close(pipefd[0]);
                int ws = 0;
                while (waitpid(pid, &ws, 0) < 0 && errno == EINTR) {}
            }
        }
    }
    --g_cs_depth;
    // strip trailing newlines, like real shells do
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

static std::string var_value(const std::string& name, ShellState& st) {
    if (name == "?") return std::to_string(st.last_status);
    if (name == "$") return std::to_string(getpid());
    if (name == "!") return std::to_string(st.last_bg_pid);
    auto it = st.vars.find(name);
    if (it != st.vars.end()) return it->second;
    const char* e = std::getenv(name.c_str());
    return e ? e : "";
}

std::string expand_one(const std::string& word, ShellState& st, QuoteKind qk) {
    if (qk == QuoteKind::Single) return word;  // '...' is fully literal
    std::string in = word;

    // Pass A — expand $vars and $(...); carry backslash-escape pairs intact
    // so pass B can decode them exactly once (protects \$, \*, \n, ...).
    std::string mid;
    for (size_t i = 0; i < in.size();) {
        char c = in[i];
        if (c == '\\' && i + 1 < in.size()) {
            mid += '\\'; mid += in[i + 1];
            i += 2;
            continue;
        }
        if (c == '~' && i == 0 && (in.size() == 1 || in[1] == '/')) {
            mid += home_dir();
            ++i;
            continue;
        }
        if (c == '$' && i + 1 < in.size()) {
            if (in[i + 1] == '(') {
                // $(...) — balanced-paren, quote-aware scan
                size_t depth = 1, j = i + 2;
                bool sq = false, dq = false;
                while (j < in.size() && depth > 0) {
                    char d = in[j];
                    if (sq) { if (d == '\'') sq = false; }
                    else if (dq) {
                        if (d == '"') dq = false;
                        else if (d == '\\' && j + 1 < in.size()) ++j;
                    } else {
                        if (d == '\'') sq = true;
                        else if (d == '"') dq = true;
                        else if (d == '(') ++depth;
                        else if (d == ')') --depth;
                    }
                    ++j;
                }
                if (depth == 0) {
                    mid += run_captured(in.substr(i + 2, j - (i + 2) - 1), st);
                    i = j;
                    continue;
                }
                // unbalanced: '$' stays literal
            } else if (in[i + 1] == '{') {
                size_t e = in.find('}', i + 2);
                if (e == std::string::npos) { mid += c; ++i; continue; }
                mid += var_value(in.substr(i + 2, e - (i + 2)), st);
                i = e + 1;
                continue;
            } else if (in[i + 1] == '?' || in[i + 1] == '$' || in[i + 1] == '!') {
                mid += var_value(in.substr(i + 1, 1), st);
                i += 2;
                continue;
            } else if (std::isalpha((unsigned char)in[i + 1]) || in[i + 1] == '_') {
                size_t j = i + 1;
                while (j < in.size() &&
                       (std::isalnum((unsigned char)in[j]) || in[j] == '_')) ++j;
                mid += var_value(in.substr(i + 1, j - (i + 1)), st);
                i = j;
                continue;
            }
        }
        mid += c;
        ++i;
    }

    // Pass B — decode escapes. Unquoted words: any \X loses the backslash.
    // Double-quoted words: only \$ \" \\ \` lose the backslash; \n etc.
    // stay literal, matching bash/fish.
    std::string out;
    for (size_t i = 0; i < mid.size(); ++i) {
        char c = mid[i];
        if (c == '\\' && i + 1 < mid.size()) {
            char d = mid[i + 1];
            if (qk == QuoteKind::Double) {
                if (d == '$' || d == '"' || d == '\\' || d == '`') { out += d; ++i; continue; }
                out += '\\';  // keep \X verbatim; X is appended next iteration
                continue;
            }
            out += d;  // unquoted: anything may be escaped
            ++i;
            continue;
        }
        out += c;
    }
    return out;
}

// True if the *raw* word contains glob characters not preceded by a
// backslash. Glob also requires QuoteKind::None (unquoted only).
static bool raw_has_glob(const std::string& w) {
    for (size_t i = 0; i < w.size(); ++i) {
        char c = w[i];
        if (c == '\\' && i + 1 < w.size()) { ++i; continue; }  // skip escaped pair
        if (c == '*' || c == '?' || c == '[') return true;
    }
    return false;
}

std::vector<std::string> expand_args(const std::vector<std::string>& words,
                                     const std::vector<QuoteSpec>& qspecs,
                                     ShellState& st) {
    std::vector<std::string> out;
    for (size_t k = 0; k < words.size(); ++k) {
        std::string e = expand_one(words[k], st, qspecs[k].kind);
        // Empty unquoted results ($(true), unset vars) drop out of the arg
        // list, like bash word splitting; quoted empties ("", '', "$(x)")
        // stay as real args.
        if (e.empty() && !qspecs[k].had_quotes) continue;
        bool has_glob = qspecs[k].kind == QuoteKind::None &&
                        raw_has_glob(words[k]);
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
