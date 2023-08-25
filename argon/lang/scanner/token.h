// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_TOKEN_H_
#define ARGON_LANG_SCANNER_TOKEN_H_

#include <cstddef>

#include <argon/vm/datatype/arobject.h>

namespace argon::lang::scanner {
    enum class TokenType {
        TK_NULL,
        END_OF_LINE,
        END_OF_FILE,

        BLANK,
        IDENTIFIER,
        SELF,

        LITERAL_BEGIN,
        NUMBER_BEGIN,
        NUMBER,
        U_NUMBER,
        NUMBER_BIN,
        U_NUMBER_BIN,
        NUMBER_OCT,
        U_NUMBER_OCT,
        NUMBER_HEX,
        U_NUMBER_HEX,
        NUMBER_CHR,
        DECIMAL,
        NUMBER_END,

        STRING_BEGIN,
        STRING,
        BYTE_STRING,
        RAW_STRING,
        STRING_END,

        ATOM,
        FALSE,
        NIL,
        TRUE,
        LITERAL_END,

        KEYWORD_BEGIN,
        KW_AS,
        KW_ASYNC,
        KW_ASSERT,
        KW_AWAIT,
        KW_BREAK,
        KW_CASE,
        KW_CONTINUE,
        KW_DEFAULT,
        KW_DEFER,
        KW_ELIF,
        KW_ELSE,
        KW_FALLTHROUGH,
        KW_FOR,
        KW_FROM,
        KW_FUNC,
        KW_IF,
        KW_IN,
        KW_IMPL,
        KW_IMPORT,
        KW_LET,
        KW_LOOP,
        KW_NOT,
        KW_OF,
        KW_PANIC,
        KW_PUB,
        KW_RETURN,
        KW_YIELD,
        KW_SPAWN,
        KW_STRUCT,
        KW_SWITCH,
        KW_TRAIT,
        KW_TRAP,
        KW_VAR,
        KW_WEAK,
        KEYWORD_END,

        INFIX_BEGIN,
        PLUS,
        MINUS,
        ASTERISK,
        SLASH,
        SLASH_SLASH,
        PERCENT,
        SHL,
        SHR,
        LESS,
        LESS_EQ,
        GREATER,
        GREATER_EQ,
        EQUAL_EQUAL,
        EQUAL_STRICT,
        NOT_EQUAL_STRICT,
        NOT_EQUAL,
        AMPERSAND,
        CARET,
        PIPE,
        AND,
        OR,
        INFIX_END,

        EXCLAMATION,
        LEFT_ROUND,
        RIGHT_ROUND,
        LEFT_SQUARE,
        RIGHT_SQUARE,
        LEFT_BRACES,
        RIGHT_BRACES,
        LEFT_INIT,
        COMMA,
        DOT,
        COLON,
        SCOPE,
        SEMICOLON,
        EQUAL,
        FAT_ARROW,
        QUESTION,
        QUESTION_DOT,
        ELVIS,
        NULL_COALESCING,
        PIPELINE,
        TILDE,

        ASSIGN_MUL,
        ASSIGN_ADD,
        ASSIGN_SUB,
        ASSIGN_SLASH,

        PLUS_PLUS,
        MINUS_MINUS,

        COMMENT_BEGIN,
        COMMENT,
        COMMENT_INLINE,
        COMMENT_END,

        ELLIPSIS,
        WALRUS
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

        Token() = default;

        Token(Token &other) = delete;

        ~Token() {
            vm::memory::Free(this->buffer);
        }

        Token &operator=(const Token &other) = delete;

        Token &operator=(Token &&other) noexcept {
            this->buffer = other.buffer;
            this->length = other.length;
            this->type = other.type;
            this->loc = other.loc;

            other.buffer = nullptr;

            return *this;
        }
    };

} // namespace argon::lang::scanner

#endif // !ARGON_LANG_SCANNER_TOKEN_H_
