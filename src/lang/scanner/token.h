// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_TOKEN_H_
#define ARGON_LANG_SCANNER_TOKEN_H_

#include <cstddef>

namespace argon::lang::scanner {
    enum class TokenType {
        TK_NULL,
        END_OF_LINE,
        END_OF_FILE
    };

    using Pos = size_t;

    struct Position {
        Pos column;
        Pos line;
        Pos offset;
    };

    struct Loc {
        Position start;
        Position end;
    };

    struct Token {
        unsigned char *buffer = nullptr;
        size_t length = 0;

        TokenType type = TokenType::TK_NULL;

        Loc loc{};
    };

} // namespace argon::lang::scanner

#endif // !ARGON_LANG_SCANNER_TOKEN_H_
