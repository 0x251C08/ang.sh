// expand.h — word expansion: ~, $VAR, ${VAR}, $?, $$, $!, and $(...)
// quotes already handled by the lexer. Glob (* ? [...]) via glob(3).
// Command substitution output is *not* re-split or re-globed (like "$(...)")
// — run it through the shell again if you need that.
// No command substitution nesting beyond 8 levels (guard against loops).
#pragma once
#include <string>
#include <vector>
#include "util.h"
#include "env.h"

namespace ang {

std::string expand_one(const std::string& word, ShellState& st, QuoteKind qk);
std::vector<std::string> expand_args(const std::vector<std::string>& words,
                                     const std::vector<QuoteSpec>& qspecs,
                                     ShellState& st);

}  // namespace ang
