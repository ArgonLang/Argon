// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_TOKEN_H_
#define ARGON_LANG_SCANNER_TOKEN_H_

namespace argon::lang::scanner2 {

    using Pos = long;

    enum class TokenType {
        TK_NULL,
        END_OF_FILE,
        END_OF_LINE,
        EXCLAMATION,
        EXCLAMATION_LBRACES,
        NOT_EQUAL,

        STRING_BEGIN,
        STRING,
        BYTE_STRING,
        RAW_STRING,
        STRING_END,

        INLINE_COMMENT,
        COMMENT,
        PERCENT,
        AMPERSAND,
        AND,
        LEFT_ROUND,
        RIGHT_ROUND,
        ASTERISK,
        PLUS,
        PLUS_PLUS,
        COMMA,
        MINUS,
        MINUS_MINUS,
        DOT,
        ELLIPSIS,
        SLASH,
        SLASH_SLASH,

        ASSIGNMENT_BEGIN,
        EQUAL,
        PLUS_EQ,
        MINUS_EQ,
        ASTERISK_EQ,
        SLASH_EQ,
        ASSIGNMENT_END,

        NUMBER_BEGIN,
        NUMBER,
        NUMBER_BIN,
        NUMBER_OCT,
        NUMBER_HEX,
        NUMBER_CHR,
        DECIMAL,
        NUMBER_END,

        COLON,
        SCOPE,
        SEMICOLON,

        RELATIONAL_BEGIN,
        LESS,
        LESS_EQ,
        GREATER,
        GREATER_EQ,
        RELATIONAL_END,

        SHL,
        EQUAL_EQUAL,
        ARROW,
        SHR,
        QUESTION,
        QUESTION_DOT,
        ELVIS,
        IDENTIFIER,
        LEFT_SQUARE,
        RIGHT_SQUARE,
        CARET,
        LEFT_BRACES,
        PIPE,
        OR,
        RIGHT_BRACES,
        TILDE,

        KEYWORD_BEGIN,
        AS,
        BREAK,
        CASE,
        CONTINUE,
        DEFAULT,
        DEFER,
        ELIF,
        ELSE,
        FALLTHROUGH,
        //FALSE,
        FOR,
        FROM,
        FUNC,
        //GOTO,
        IF,
        IN,
        IMPL,
        IMPORT,
        LET,
        LOOP,
        //NIL,
        PUB,
        RETURN,
        //SELF,
        SPAWN,
        STRUCT,
        SWITCH,
        TRAIT,
        //TRUE,
        VAR,
        WEAK,
        KEYWORD_END,

        FALSE,
        NIL,
        SELF,
        TRUE,

        ERROR
    };

    class Token {
    public:
        TokenType type = TokenType::TK_NULL;

        Pos start = 0;
        Pos end = 0;

        unsigned char *buf = nullptr;

        Token(TokenType type, Pos start, Pos end, const unsigned char *buf);

        Token(TokenType type, Pos start, Pos end) : Token(type, start, end, nullptr) {}

        Token() = default;

        ~Token();

        bool operator==(const Token &token) const;

        bool Clone(Token &dest) const noexcept;

        Token &operator=(Token &&token) noexcept;
    };

}  // namespace lang::scanner

#endif