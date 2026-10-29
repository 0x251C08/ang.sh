// highlight.h — fish-style inline syntax highlighting for the line editor.
// v0.1 colors: command name (green ok / red missing), strings (yellow),
// redirects + operators (cyan), comments (dim).
#pragma once
#include <string>
#include "env.h"

namespace ang {

std::string highlight_line(const std::string& line, const ShellState& st);

// History-based autosuggestion: newest history entry starting with `line`.
std::string autosuggest(const std::string& line, const ShellState& st);

}  // namespace ang
