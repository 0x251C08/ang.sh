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
    std::string out;
    for (size_t i = 0; i < in.size();) {
        char c = in[i];
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
                    out += run_captured(in.substr(i + 2, j - (i + 2) - 1), st);
                    i = j;
                    continue;
                }
                // unbalanced: '$' stays literal
            } else if (in[i + 1] == '{') {
                size_t e = in.find('}', i + 2);
                if (e == std::string::npos) { out += c; ++i; continue; }
                out += var_value(in.substr(i + 2, e - (i + 2)), st);
                i = e + 1;
                continue;
            } else if (in[i + 1] == '?' || in[i + 1] == '$' || in[i + 1] == '!') {
                out += var_value(in.substr(i + 1, 1), st);
                i += 2;
                continue;
            } else if (std::isalpha((unsigned char)in[i + 1]) || in[i + 1] == '_') {
                size_t j = i + 1;
                while (j < in.size() &&
                       (std::isalnum((unsigned char)in[j]) || in[j] == '_')) ++j;
                out += var_value(in.substr(i + 1, j - (i + 1)), st);
                i = j;
                continue;
            }
        }
        out += c;
        ++i;
    }
    return out;
}

std::vector<std::string> expand_args(const std::vector<std::string>& words,
                                     const std::vector<QuoteKind>& qkinds,
                                     ShellState& st) {
    std::vector<std::string> out;
    for (size_t k = 0; k < words.size(); ++k) {
        std::string e = expand_one(words[k], st, qkinds[k]);
        bool has_glob = qkinds[k] == QuoteKind::None &&
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
