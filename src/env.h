// env.h — shell state: variables, aliases, abbreviations, options.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace ang {

struct ShellState {
    // Variables set via `set` (ang) or `export`/assignment (posix).
    // Exported ones are also in the process environment.
    std::map<std::string, std::string> vars;
    std::map<std::string, std::string> aliases;  // bash-style: alias ll='ls -l'
    std::map<std::string, std::string> abbrs;    // fish-style: abbr g='git'
    std::vector<std::string> history;

    bool posix = false;     // false = Ang syntax (default), true = POSIX sh-like
    bool autocd = false;    // zsh-style: bare directory name cds into it
    int last_status = 0;     // $?
    pid_t last_bg_pid = 0;   // $!
    std::string shell_name = "angsh";
    std::string theme_name = "default";

    // Prompt templates (see prompt.cpp + themes/*.conf).
    std::string prompt_left;
    std::string prompt_right;

    std::string history_file;
    bool interactive = false;

    std::string get(const std::string& k, const std::string& dflt = "") const;
    void set_var(const std::string& k, const std::string& v, bool export_env);
    void unset_var(const std::string& k);
};

}  // namespace ang
