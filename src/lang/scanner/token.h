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
        END_OF_FILE,
        NUMBER,
        NUMBER_BIN,
        NUMBER_OCT,
        NUMBER_HEX,
        NUMBER_CHR,
        DECIMAL,
        STRING,
        BYTE_STRING,
        RAW_STRING,
        IDENTIFIER,
        BLANK,

        KW_AS,
        KW_ASSERT,

        ATOM,
        EXCLAMATION,
        NOT_EQUAL,
        NOT_EQUAL_STRICT,
        PERCENT,
        AMPERSAND,
        AND,
        LEFT_ROUND,
        RIGHT_ROUND,
        LEFT_SQUARE,
        RIGHT_SQUARE,
        LEFT_BRACES,
        RIGHT_BRACES,
        ASTERISK,
        COMMA,
        DOT,
        COLON,
        SCOPE,
        SEMICOLON,
        EQUAL,
        EQUAL_EQUAL,
        EQUAL_STRICT,
        FAT_ARROW,
        QUESTION,
        QUESTION_DOT,
        ELVIS,
        OR,
        PIPELINE,
        TILDE,
        CARET,
        PIPE,

        ASSIGN_MUL,
        ASSIGN_ADD,
        ASSIGN_SUB,
        ASSIGN_SLASH,

        GREATER_EQ,
        LESS_EQ,
        SHL,
        SHR,
        GREATER,
        LESS,

        PLUS,
        PLUS_PLUS,
        MINUS,
        MINUS_MINUS,
        SLASH,
        SLASH_SLASH,

        COMMENT,
        COMMENT_INLINE,

        ELLIPSIS
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
