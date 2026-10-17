// editor.cpp
#include "editor.h"
#include "highlight.h"
#include "prompt.h"
#include "util.h"
#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace ang {
namespace {

termios g_orig{};
bool g_raw = false;

void disable_raw() {
    if (g_raw) { tcsetattr(STDIN_FILENO, TCSADRAIN, &g_orig); g_raw = false; }
}
void enable_raw() {
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &g_orig);
    termios r = g_orig;
    r.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    r.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    r.c_oflag &= ~(OPOST);
    r.c_cc[VMIN] = 1; r.c_cc[VTIME] = 0;
    // TCSADRAIN (not TCSAFLUSH): never discard input typed while loading.
    tcsetattr(STDIN_FILENO, TCSADRAIN, &r);
    g_raw = true;
}

int term_width() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

// Complete `prefix`: builtins + aliases + PATH commands + files.
std::vector<std::string> complete(const std::string& prefix, ShellState& st,
                                  bool first_word) {
    std::vector<std::string> out;
    auto add = [&](const std::string& c) {
        if (c.size() >= prefix.size() && c.compare(0, prefix.size(), prefix) == 0)
            if (std::find(out.begin(), out.end(), c) == out.end()) out.push_back(c);
    };
    if (first_word) {
        for (auto b : {"cd","exit","export","unset","set","alias","unalias","abbr",
                       "posix","source","history","help","version","true","false",
                       "type","theme","env","echo","pwd","autocd"})
            add(b);
        for (auto& kv : st.aliases) add(kv.first);
        for (auto& kv : st.abbrs) add(kv.first);
        const char* p = getenv("PATH");
        std::string path = p ? p : "/bin:/usr/bin";
        size_t a = 0;
        while (a <= path.size()) {
            size_t b = path.find(':', a);
            std::string d = path.substr(a, b == std::string::npos ? b : b - a);
            DIR* dp = opendir(d.c_str());
            if (dp) {
                while (dirent* e = readdir(dp)) {
                    std::string n = e->d_name;
                    if (!n.empty() && n[0] != '.') add(n);
                }
                closedir(dp);
            }
            if (b == std::string::npos) break;
            a = b + 1;
        }
    }
    // files always
    std::string dir = ".", pre = prefix;
    size_t sl = prefix.find_last_of('/');
    if (sl != std::string::npos) {
        dir = prefix.substr(0, sl + 1);
        if (dir.empty()) dir = "/";
        pre = prefix.substr(sl + 1);
    }
    DIR* dp = opendir(dir.c_str());
    if (dp) {
        while (dirent* e = readdir(dp)) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            if (n.size() >= pre.size() && n.compare(0, pre.size(), pre) == 0) {
                std::string full = (sl == std::string::npos) ? n : prefix.substr(0, sl + 1) + n;
                if (e->d_type == DT_DIR) full += "/";
                add(full);
            }
        }
        closedir(dp);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void render(const std::string& left, const std::string& right,
            const std::string& buf, size_t cursor, const std::string& ghost,
            const ShellState& st) {
    std::string hl = highlight_line(buf, st);
    // NOTE: cursor positioning with multi-byte highlight codes is approximate;
    // v0.1 keeps the cursor at the end-of-render + backoff by suffix length.
    std::string out = "\r\033[K" + left + hl + "\033[2m" + ghost + "\033[0m";
    // right prompt: pad to terminal width
    if (!right.empty()) {
        size_t used = visible_len(left) + buf.size() + ghost.size();
        size_t rw = visible_len(right);
        int w = term_width();
        if (used + rw + 1 < (size_t)w)
            out += std::string(w - used - rw, ' ') + "\033[2m" + right + "\033[0m";
    }
    // move cursor back: ghost + (buf.size - cursor)
    size_t back = ghost.size() + (buf.size() - cursor);
    if (back) out += "\033[" + std::to_string(back) + "D";
    printf("%s", out.c_str());
    fflush(stdout);
}

}  // namespace

bool read_line(ShellState& st, const std::string& left,
               const std::string& right, std::string* out) {
    if (!isatty(STDIN_FILENO)) {
        printf("%s", left.c_str());
        fflush(stdout);
        if (!std::getline(std::cin, *out)) return false;
        return true;
    }
    enable_raw();
    std::string buf;
    size_t cursor = 0;
    size_t hist_idx = st.history.size();  // one past end
    std::string hist_saved;
    bool first_hist = true;

    render(left, right, buf, cursor, autosuggest(buf, st), st);
    while (true) {
        char c = 0;
        if (::read(STDIN_FILENO, &c, 1) != 1) { disable_raw(); return false; }
        if (c == '\r' || c == '\n') {
            printf("\r\n");
            fflush(stdout);
            disable_raw();
            *out = buf;
            return true;
        }
        if (c == 4) {  // Ctrl-D
            if (buf.empty()) { printf("\r\n"); disable_raw(); return false; }
            continue;
        }
        if (c == 127 || c == 8) {  // backspace
            if (cursor > 0) { buf.erase(cursor - 1, 1); --cursor; }
        } else if (c == 27) {  // escape sequence
            char s1 = 0, s2 = 0;
            if (::read(STDIN_FILENO, &s1, 1) != 1) continue;
            if (s1 != '[') continue;
            if (::read(STDIN_FILENO, &s2, 1) != 1) continue;
            std::string ghost = autosuggest(buf, st);
            if (s2 == 'D') { if (cursor > 0) --cursor; }          // Left
            else if (s2 == 'C') {                                 // Right
                if (cursor < buf.size()) ++cursor;
                else if (!ghost.empty()) { buf += ghost; cursor = buf.size(); }
            } else if (s2 == 'A') {                               // Up
                if (!st.history.empty() && hist_idx > 0) {
                    if (first_hist) { hist_saved = buf; first_hist = false; }
                    --hist_idx;
                    buf = st.history[hist_idx]; cursor = buf.size();
                }
            } else if (s2 == 'B') {                               // Down
                if (hist_idx < st.history.size()) {
                    ++hist_idx;
                    buf = (hist_idx == st.history.size()) ? (first_hist ? buf : hist_saved) : st.history[hist_idx];
                    cursor = buf.size();
                }
            } else if (s2 == 'H') cursor = 0;
            else if (s2 == 'F') cursor = buf.size();
            else if (s2 == '3') {                                 // Delete
                char t = 0;
                ::read(STDIN_FILENO, &t, 1);  // consume '~'
                if (cursor < buf.size()) buf.erase(cursor, 1);
            }
        } else if (c == 1) cursor = 0;                             // Ctrl-A
        else if (c == 5) cursor = buf.size();                       // Ctrl-E
        else if (c == 21) { buf.erase(0, cursor); cursor = 0; }     // Ctrl-U
        else if (c == 11) buf.erase(cursor);                       // Ctrl-K
        else if (c == 23) {                                        // Ctrl-W
            size_t e = cursor;
            while (e > 0 && buf[e - 1] == ' ') --e;
            while (e > 0 && buf[e - 1] != ' ') --e;
            buf.erase(e, cursor - e); cursor = e;
        } else if (c == 6) {                                       // Ctrl-F accept ghost
            std::string g = autosuggest(buf, st);
            if (!g.empty()) { buf += g; cursor = buf.size(); }
        } else if (c == '\t') {                                   // Tab
            size_t s = buf.find_last_of(" \t;");
            std::string pre = (s == std::string::npos) ? buf : buf.substr(s + 1);
            bool first = (s == std::string::npos);
            auto cands = complete(pre, st, first);
            if (cands.size() == 1) {
                buf.replace(buf.size() - pre.size(), pre.size(), cands[0]);
                cursor = buf.size();
            } else if (cands.size() > 1) {
                disable_raw();
                printf("\r\n");
                for (auto& cd : cands) printf("%s  ", cd.c_str());
                printf("\r\n");
                enable_raw();
            }
        } else if (c == 3) {                                       // Ctrl-C
            printf("\r\n");
            disable_raw();
            *out = "";
            st.last_status = 130;
            return true;  // empty line, like bash ^C
        } else if (c >= 32) {
            buf.insert(cursor, 1, c);
            ++cursor;
        }
        render(left, right, buf, cursor, autosuggest(buf, st), st);
    }
}

void save_history(const ShellState& st) {
    if (st.history_file.empty()) return;
    std::string dir = data_dir();
    mkdir(dir.c_str(), 0755);
    // create parents if needed (best effort)
    std::ofstream o(st.history_file);
    if (!o) return;
    size_t from = st.history.size() > 5000 ? st.history.size() - 5000 : 0;
    for (size_t i = from; i < st.history.size(); ++i) o << st.history[i] << "\n";
}

}  // namespace ang
