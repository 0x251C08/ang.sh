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
            continue;
        }
        if (c == '2' && i + 1 < n && line[i + 1] == '>') {
            if (i + 2 < n && line[i + 2] == '>') { push(TokKind::RedirErrAppend, "2>>"); i += 3; }
            else { push(TokKind::RedirErr, "2>"); i += 2; }
            continue;
        }
        // Word (with quoting)
        std::string w;
        bool quoted = false;
        bool in_single = false, in_double = false;
        bool any = false;
        while (i < n) {
            char d = line[i];
            if (in_single) {
                if (d == '\'') { in_single = false; ++i; }
                else { w += d; ++i; }
                continue;
            }
            if (in_double) {
                if (d == '"') { in_double = false; ++i; }
                else if (d == '\\' && i + 1 < n) { w += line[i + 1]; i += 2; }
                else { w += d; ++i; }
                continue;
            }
            if (d == '\'') { in_single = true; quoted = true; ++i; any = true; continue; }
            if (d == '"') { in_double = true; quoted = true; ++i; any = true; continue; }
            if (d == '\\' && i + 1 < n) { w += line[i + 1]; quoted = true; i += 2; any = true; continue; }
            if (d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == ';' || d == '|' || d == '&' || d == '<' || d == '>' || d == '#') break;
            w += d; ++i; any = true;
        }
        if (in_single || in_double) {
            if (err) *err = "unterminated quote";
            return {};
        }
        if (any || quoted) {
            // bash-style `2>` / `2>>`: an unquoted word that is exactly "2"
            // immediately followed by '>' is a stderr redirect, not an arg.
            if (!quoted && w == "2" && i < n && line[i] == '>') {
                if (i + 1 < n && line[i + 1] == '>') { push(TokKind::RedirErrAppend, "2>>"); i += 2; }
                else { push(TokKind::RedirErr, "2>"); ++i; }
                continue;
            }
            Token t; t.kind = TokKind::Word; t.text = w; t.quoted = quoted;
            out.push_back(t);
        }
    }
    return out;
}

}  // namespace ang
