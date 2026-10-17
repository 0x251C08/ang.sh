// expand.h — word expansion: ~, $VAR, ${VAR}, $?, $$, $!, quotes already
// handled by the lexer. Glob (* ? [...]) via glob(3). No command
// substitution in v0.1 (roadmap).
#pragma once
#include <string>
#include <vector>
#include "env.h"

namespace ang {

std::string expand_one(const std::string& word, ShellState& st, bool quoted);
std::vector<std::string> expand_args(const std::vector<std::string>& words,
                                     const std::vector<bool>& quoted,
                                     ShellState& st);

}  // namespace ang
