// lexer.cpp
#include "lexer.h"

namespace ang {

std::vector<Token> lex(const std::string& line, std::string* err) {
    std::vector<Token> out;
    size_t i = 0, n = line.size();
    auto push = [&](TokKind k, const std::string& t) {
        Token tok; tok.kind = k; tok.text = t; out.push_back(tok);
    };
    while (i < n) {
        char c = line[i];
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        if (c == '\n') { push(TokKind::Semi, ";"); ++i; continue; }
        if (c == '#') break;  // comment to end of line
        if (c == ';') { push(TokKind::Semi, ";"); ++i; continue; }
        if (c == '|') {
            if (i + 1 < n && line[i + 1] == '|') { push(TokKind::Or, "||"); i += 2; }
            else { push(TokKind::Pipe, "|"); ++i; }
            continue;
        }
        if (c == '&') {
            if (i + 1 < n && line[i + 1] == '>') {
                if (i + 2 < n && line[i + 2] == '>') { push(TokKind::RedirBothAppend, "&>>"); i += 3; }
                else { push(TokKind::RedirBoth, "&>"); i += 2; }
            } else if (i + 1 < n && line[i + 1] == '&') { push(TokKind::And, "&&"); i += 2; }
            else { push(TokKind::Bg, "&"); ++i; }
            continue;
        }
        if (c == '<') { push(TokKind::RedirIn, "<"); ++i; continue; }
        if (c == '>') {
            if (i + 1 < n && line[i + 1] == '>') { push(TokKind::RedirAppend, ">>"); i += 2; }
            else { push(TokKind::RedirOut, ">"); ++i; }
            // >&2 — stdout dup'd to stderr (POSIX), handled as op + target
            if (i < n && line[i] == '&' && i + 1 < n && line[i + 1] == '2') {
                push(TokKind::Word, "&2"); i += 2;
            }
            continue;
        }
        if (c == '2' && i + 1 < n && line[i + 1] == '>') {
            if (i + 2 < n && line[i + 2] == '>') { push(TokKind::RedirErrAppend, "2>>"); i += 3; }
            else { push(TokKind::RedirErr, "2>"); i += 2; }
            // 2>&1 — stderr dup'd to stdout (POSIX), handled as op + target
            if (i < n && line[i] == '&' && i + 1 < n && line[i + 1] == '1') {
                push(TokKind::Word, "&1"); i += 2;
            }
            continue;
        }
        // Word (with quoting)
        std::string w;
        QuoteKind qk = QuoteKind::None;
        bool in_single = false, in_double = false;
        bool any = false;
        bool saw_quote = false;   // literal quote chars appeared in this word
        while (i < n) {
            char d = line[i];
            if (in_single) {
                if (d == '\'') { in_single = false; ++i; }
                else { w += d; ++i; }
                continue;
            }
            if (in_double) {
                if (d == '"') { in_double = false; ++i; }
                // keep escape pairs intact; expansion decodes \$ \" \\ \` only
                else if (d == '\\' && i + 1 < n &&
                         (line[i + 1] == '$' || line[i + 1] == '"' ||
                          line[i + 1] == '\\' || line[i + 1] == '`')) {
                    w += '\\'; w += line[i + 1]; i += 2;
                } else { w += d; ++i; }
                continue;
            }
            if (d == '\'') { in_single = true; qk = QuoteKind::Single; saw_quote = true; ++i; any = true; continue; }
            if (d == '"') { in_double = true; qk = QuoteKind::Double; saw_quote = true; ++i; any = true; continue; }
            // keep escape pairs so expansion can protect the char (e.g. \$, \*);
            // quote kind stays None — decoding happens in expansion, and glob
            // is skipped via raw_has_glob() on escaped glob characters
            if (d == '\\' && i + 1 < n) { w += '\\'; w += line[i + 1]; i += 2; any = true; continue; }
            if (!in_single && d == '$' && i + 1 < n && line[i + 1] == '(') {
                // command substitution $(...): scan to matching ')', quote-aware
                size_t depth = 1, j = i + 2;
                bool sq = false, dq = false;
                while (j < n && depth > 0) {
                    char c = line[j];
                    if (sq) { if (c == '\'') sq = false; }
                    else if (dq) {
                        if (c == '"') dq = false;
                        else if (c == '\\' && j + 1 < n) ++j;
                    } else {
                        if (c == '\'') sq = true;
                        else if (c == '"') dq = true;
                        else if (c == '(') ++depth;
                        else if (c == ')') --depth;
                    }
                    ++j;
                }
                if (depth == 0) {
                    w += line.substr(i, j - i);   // keep the $(...) text verbatim
                    qk = QuoteKind::Double;       // expands; output is not re-globed/re-split
                    i = j;
                    any = true;
                    continue;
                }
                // unbalanced: '$' stays literal
            }
            if (d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == ';' || d == '|' || d == '&' || d == '<' || d == '>' || (d == '#' && w.empty())) break;
            w += d; ++i; any = true;
        }
        if (in_single || in_double) {
            if (err) *err = "unterminated quote";
            return {};
        }
        if (any || qk != QuoteKind::None) {
            // bash-style `2>` / `2>>`: an unquoted word that is exactly "2"
            // immediately followed by '>' is a stderr redirect, not an arg.
            if (qk == QuoteKind::None && w == "2" && i < n && line[i] == '>') {
                if (i + 1 < n && line[i + 1] == '>') { push(TokKind::RedirErrAppend, "2>>"); i += 2; }
                else { push(TokKind::RedirErr, "2>"); ++i; }
                if (i < n && line[i] == '&' && i + 1 < n && line[i + 1] == '1') {
                    push(TokKind::Word, "&1"); i += 2;
                }
                continue;
            }
            Token t; t.kind = TokKind::Word; t.text = w; t.quote_kind = qk;
            t.had_quotes = saw_quote;
            out.push_back(t);
        }
    }
    return out;
}

}  // namespace ang
