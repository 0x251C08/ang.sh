// lexer.h — splits one input line into tokens.
// v0.1 token set is intentionally small: words, pipes, logic ops,
// separators, background, and redirections. No subshells yet.
#pragma once
#include <string>
#include <vector>

namespace ang {

enum class TokKind {
    Word,          // command / argument (quotes already stripped)
    Pipe,          // |
    And,           // &&
    Or,            // ||
    Semi,          // ;
    Bg,            // & (background)
    RedirIn,       // <
    RedirOut,      // >
    RedirAppend,   // >>
    RedirErr,      // 2>
    RedirErrAppend,// 2>>
    RedirBoth,     // &>
    RedirBothAppend,// &>>
    End
};

struct Token {
    TokKind kind = TokKind::End;
    std::string text;  // Word text, or operator spelling
    bool quoted = false; // true if any part was single/double quoted (skips glob)
};

std::vector<Token> lex(const std::string& line, std::string* err = nullptr);

}  // namespace ang
