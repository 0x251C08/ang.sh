// executor.cpp
#include "executor.h"
#include "builtins.h"
#include "expand.h"
#include "util.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace ang {

bool g_should_exit = false;
int g_exit_code = 0;

// Parent builtins (cd/posix/...) print via iostreams while external commands
// run in forked children. Without a flush, a child would inherit buffered
// parent output and print it a second time at _exit. Flush before every fork.
static void flush_all() {
    std::cout.flush();
    std::cerr.flush();
    fflush(nullptr);
}

// --- interactive pre-processing: history expansion, alias, abbr, autocd ---

std::string preprocess(const std::string& line, ShellState& st, bool* skip) {
    *skip = false;
    std::string s = trim(line);
    if (s.empty() || s[0] == '#') { *skip = true; return ""; }

    // bash-style history expansion: !! and !$ (interactive only)
    if (st.interactive && !st.history.empty()) {
        if (s == "!!") s = st.history.back();
        else if (s.size() >= 2 && s.substr(s.size() - 2) == "!!") {
            // trailing !! -> substitute (simple form)
            s = s.substr(0, s.size() - 2) + st.history.back();
        } else {
            size_t p = s.find("!$");
            if (p != std::string::npos) {
                auto w = split_words(st.history.back());
                s.replace(p, 2, w.empty() ? "" : w.back());
            }
        }
    }

    // fish-style abbreviation: expand first word only
    {
        auto w = split_words(s);
        if (!w.empty()) {
            auto it = st.abbrs.find(w[0]);
            if (it != st.abbrs.end()) {
                s = it->second + s.substr(w[0].size());
                w = split_words(s);
            }
        }
        // bash-style alias: expand first word (loop, max 10)
        for (int k = 0; k < 10 && !w.empty(); ++k) {
            auto it = st.aliases.find(w[0]);
            if (it == st.aliases.end()) break;
            s = it->second + s.substr(w[0].size());
            w = split_words(s);
        }
    }
    return s;
}

// --- redirections ---

static bool apply_redirs(const std::vector<Redir>& rs, std::string* err) {
    for (const auto& r : rs) {
        int fd = -1;
        if (r.op == "<") fd = open(r.target.c_str(), O_RDONLY);
        else if (r.op == ">") fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        else if (r.op == ">>") fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        else if (r.op == "2>" || r.op == "&>") fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        else if (r.op == "2>>" || r.op == "&>>") fd = open(r.target.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            if (err) *err = "cannot open '" + r.target + "': " + strerror(errno);
            return false;
        }
        if (r.op == "<") dup2(fd, STDIN_FILENO);
        else if (r.op == "2>" || r.op == "2>>") dup2(fd, STDERR_FILENO);
        else if (r.op == "&>" || r.op == "&>>") { dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); }
        else dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return true;
}

// --- pipeline execution ---

static int run_pipeline(const Pipeline& pl, ShellState& st) {
    // Ang mode rejects POSIX leading assignments at *run* time (not parse
    // time) so `posix on; FOO=bar` and `set --posix on; FOO=bar` work
    // left-to-right within a single line.
    if (!st.posix) {
        for (const auto& c : pl.cmds) {
            if (!c.assigns.empty()) {
                const std::string& w = c.assigns[0];
                std::string n = w.substr(0, w.find('='));
                std::string v = w.substr(w.find('=') + 1);
                fprintf(stderr,
                    "%sassignment '%s' needs POSIX mode "
                    "(try `set %s %s` or `posix on`)\n",
                    err_prefix().c_str(), w.c_str(), n.c_str(), v.c_str());
                st.last_status = 2;
                return 2;
            }
        }
    }
    // Parent-run builtin fast path: single command, no pipe, no redir-fork.
    // (Redirections on builtins still apply: we fork for those to keep it simple.)
    if (pl.cmds.size() == 1 && !pl.background && pl.cmds[0].redirs.empty()) {
        const SimpleCmd& c = pl.cmds[0];
        // POSIX temp assignments without command: `FOO=bar` alone
        if (c.args.empty() && !c.assigns.empty()) {
            for (auto& a : c.assigns) {
                std::string n, v; is_assign_word(a, &n, &v);
                st.set_var(n, expand_one(v, st, QuoteKind::None), true);
            }
            st.last_status = 0;
            return 0;
        }
        if (!c.args.empty()) {
            std::vector<std::string> expanded =
                expand_args(c.args, c.quoted, st);
            // apply POSIX leading assigns as temp env for this command
            for (auto& a : c.assigns) {
                std::string n, v; is_assign_word(a, &n, &v);
                expanded.insert(expanded.begin(), n + "=" + expand_one(v, st, QuoteKind::None));
            }
            // split VAR=x prefixes back out for the child env
            std::vector<std::string> tmps;
            while (!expanded.empty()) {
                std::string n, v;
                if (is_assign_word(expanded[0], &n, &v)) {
                    tmps.push_back(expanded[0]);
                    expanded.erase(expanded.begin());
                } else break;
            }
            if (!expanded.empty() && is_parent_builtin(expanded[0])) {
                for (auto& t : tmps) {
                    std::string n, v; is_assign_word(t, &n, &v);
                    setenv(n.c_str(), v.c_str(), 1);
                }
                bool handled = false;
                int rc = run_builtin(expanded, st, &handled);
                (void)handled;
                st.last_status = rc;
                return rc;
            }
        }
    }

    // General path: fork each stage, wire pipes.
    size_t n = pl.cmds.size();
    std::vector<pid_t> kids;
    int prev_read = -1;

    auto run_bg = pl.background;
    pid_t runner = -1;
    if (run_bg) {
        flush_all();
        runner = fork();
        if (runner < 0) { perror("fork"); return 1; }
        if (runner != 0) {
            // parent: don't wait; print job pid (zsh/bash style).
            // Track the job so it can be reaped and reported later.
            st.last_bg_pid = runner;
            if (prev_read >= 0) close(prev_read);
            fprintf(stderr, "[bg] %d\n", (int)runner);
            std::string cmdline;
            for (auto& s : pl.cmds[0].args) {
                if (!cmdline.empty()) cmdline += " ";
                cmdline += s;
            }
            st.bg_jobs.push_back({runner, cmdline, false, 0});
            st.last_status = 0;
            return 0;
        }
        // child continues and runs the pipeline synchronously below,
        // then exits with the pipeline status.
    }

    for (size_t k = 0; k < n; ++k) {
        int fds[2] = {-1, -1};
        bool has_next = (k + 1 < n);
        if (has_next && pipe(fds) < 0) { perror("pipe"); return 1; }

        flush_all();
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            // child
            signal(SIGINT, SIG_DFL);
            if (prev_read >= 0) { dup2(prev_read, STDIN_FILENO); close(prev_read); }
            if (has_next) { close(fds[0]); dup2(fds[1], STDOUT_FILENO); close(fds[1]); }
            // env assigns
            const SimpleCmd& c = pl.cmds[k];
            for (auto& a : c.assigns) {
                std::string nn, vv; is_assign_word(a, &nn, &vv);
                setenv(nn.c_str(), expand_one(vv, st, QuoteKind::None).c_str(), 1);
            }
            std::vector<std::string> expanded =
                expand_args(c.args, c.quoted, st);
            // strip VAR=x prefixes (already setenv'd)
            while (!expanded.empty()) {
                std::string nn, vv;
                if (is_assign_word(expanded[0], &nn, &vv)) {
                    setenv(nn.c_str(), vv.c_str(), 1);
                    expanded.erase(expanded.begin());
                } else break;
            }
            std::string rerr;
            if (!apply_redirs(c.redirs, &rerr)) {
                fprintf(stderr, "%s%s\n", err_prefix().c_str(), rerr.c_str());
                _exit(1);
            }
            if (expanded.empty()) _exit(0);
            bool handled = false;
            // Builtins that don't need parent state can run in the child.
            if (!is_parent_builtin(expanded[0])) {
                int rc = run_builtin(expanded, st, &handled);
                if (handled) fflush(nullptr), _exit(rc);
            } else {
                int rc = run_builtin(expanded, st, &handled);
                if (handled) fflush(nullptr), _exit(rc);
            }
            std::vector<char*> argv;
            for (auto& s : expanded) argv.push_back(const_cast<char*>(s.c_str()));
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            fprintf(stderr, "%scommand not found: %s\n", err_prefix().c_str(),
                    expanded[0].c_str());
            _exit(127);
        }
        kids.push_back(pid);
        if (prev_read >= 0) close(prev_read);
        if (has_next) { close(fds[1]); prev_read = fds[0]; }
    }

    int status = 0;
    for (pid_t p : kids) {
        int ws = 0;
        while (waitpid(p, &ws, 0) < 0 && errno == EINTR) {}
        if (p == kids.back()) {
            status = WIFEXITED(ws) ? WEXITSTATUS(ws) : 128 + WTERMSIG(ws);
        }
    }
    if (run_bg) {
        fflush(nullptr);
        _exit(status);  // runner child exits; parent already returned
    }
    st.last_status = status;
    return status;
}

int run_jobs(const JobList& jobs, ShellState& st) {
    int last = st.last_status;
    for (const auto& item : jobs.items) {
        for (size_t k = 0; k < item.pipes.size(); ++k) {
            // && / || short-circuit
            if (k > 0) {
                const std::string& op = item.ops[k - 1];
                if (op == "&&" && last != 0) continue;
                if (op == "||" && last == 0) continue;
            }
            last = run_pipeline(item.pipes[k], st);
            if (g_should_exit) return g_exit_code;
        }
    }
    return last;
}

}  // namespace ang
