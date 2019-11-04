// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_TOKEN_H_
#define ARGON_LANG_SCANNER_TOKEN_H_

#include <memory>
#include <string>
#include <map>

namespace lang::scanner {

    enum class TokenType {
        END_OF_FILE,
        END_OF_LINE,
        EXCLAMATION,
        NOT_EQUAL,
        STRING,
        BYTE_STRING,
        RAW_STRING,
        INLINE_COMMENT,
        COMMENT,
        PERCENT,
        AMPERSAND,
        AND,
        // SINGLE QUOTE
        LEFT_ROUND,
        RIGHT_ROUND,
        ASTERISK,
        ASTERISK_EQ,
        PLUS,
        PLUS_PLUS,
        PLUS_EQ,
        COMMA,
        MINUS,
        MINUS_MINUS,
        MINUS_EQ,
        DOT,
        ELLIPSIS,
        FRACTION_SLASH,
        SLASH_EQ,
        NUMBER_BIN,
        NUMBER_OCT,
        NUMBER_HEX,
        NUMBER,
        DECIMAL,
        COLON,
        SEMICOLON,
        LESS,
        SHL,
        LESS_EQ,
        EQUAL,
        EQUAL_EQUAL,
        GREATER,
        SHR,
        GREATER_EQ,
        WORD,
        LEFT_SQUARE,
        RIGHT_SQUARE,
        CARET,
        // `
                LEFT_BRACES,
        PIPE,
        OR,
        RIGHT_BRACES,
        TILDE,
        ERROR
    };

    static const std::map<TokenType, const char *> TokenStringValue = {
            {TokenType::END_OF_FILE,    "EOF"},
            {TokenType::END_OF_LINE,    "EOL"},
            {TokenType::EXCLAMATION,    "EXCLAMATION"},
            {TokenType::NOT_EQUAL,      "NOT_EQUAL"},
            {TokenType::STRING,         "STRING"},
            {TokenType::BYTE_STRING,    "BYTE_STRING"},
            {TokenType::RAW_STRING,     "RAW_STRING"},
            {TokenType::INLINE_COMMENT, "INLINE_COMMENT"},
            {TokenType::COMMENT,        "COMMENT"},
            {TokenType::PERCENT,        "PERCENT"},
            {TokenType::AMPERSAND,      "AMPERSAND"},
            {TokenType::AND,            "AND"},
            // SINGLE QUOTE
            {TokenType::LEFT_ROUND,     "LEFT_ROUND"},
            {TokenType::RIGHT_ROUND,    "RIGHT_ROUND"},
            {TokenType::ASTERISK,       "ASTERISK"},
            {TokenType::ASTERISK_EQ,    "ASTERISK_EQ"},
            {TokenType::PLUS,           "PLUS"},
            {TokenType::PLUS_PLUS,      "PLUS_PLUS"},
            {TokenType::PLUS_EQ,        "PLUS_EQ"},
            {TokenType::COMMA,          "COMMA"},
            {TokenType::MINUS,          "MINUS"},
            {TokenType::MINUS_MINUS,    "MINUS_MINUS"},
            {TokenType::MINUS_EQ,       "MINUS_EQ"},
            {TokenType::DOT,            "DOT"},
            {TokenType::ELLIPSIS,       "ELLIPSIS"},
            {TokenType::FRACTION_SLASH, "SLASH"},
            {TokenType::SLASH_EQ,       "SLASH_EQ"},
            {TokenType::NUMBER_BIN,     "NUMBER_BIN"},
            {TokenType::NUMBER_OCT,     "NUMBER_OCT"},
            {TokenType::NUMBER_HEX,     "NUMBER_HEX"},
            {TokenType::NUMBER,         "NUMBER"},
            {TokenType::DECIMAL,        "DECIMAL"},
            {TokenType::COLON,          "COLON"},
            {TokenType::SEMICOLON,      "SEMICOLON"},
            {TokenType::LESS,           "LESS"},
            {TokenType::SHL,            "SHL"},
            {TokenType::LESS_EQ,        "LESS_EQ"},
            {TokenType::EQUAL,          "EQUAL"},
            {TokenType::EQUAL_EQUAL,    "EQUAL_EQUAL"},
            {TokenType::GREATER,        "GREATER"},
            {TokenType::SHR,            "SHR"},
            {TokenType::GREATER_EQ,     "GREATER_EQ"},
            {TokenType::WORD,           "WORD"},
            {TokenType::LEFT_SQUARE,    "LEFT_SQUARE"},
            {TokenType::RIGHT_SQUARE,   "RIGHT_SQUARE"},
            {TokenType::CARET,          "CARET"},
            // `
            {TokenType::LEFT_BRACES,    "LEFT_BRACES"},
            {TokenType::PIPE,           "PIPE"},
            {TokenType::OR,             "OR"},
            {TokenType::RIGHT_BRACES,   "RIGHT_BRACES"},
            {TokenType::TILDE,          "TILDE"},
            {TokenType::ERROR,          "ERROR"}
    };

    struct Token {
        TokenType type = TokenType::END_OF_FILE;
        unsigned colno = 0;
        unsigned lineno = 0;
        std::string value;

        Token(Token &&) = default;

        Token(Token &) = default;

        Token() = default;

        Token(TokenType type, unsigned colno, unsigned lineno, const std::string &value) {
            this->type = type;
            this->colno = colno;
            this->lineno = lineno;
            this->value = value;
        }

        Token &operator=(Token &&token) noexcept {
            this->type = token.type;
            this->colno = token.colno;
            this->lineno = token.lineno;
            this->value = token.value;
            return *this;
        }

        bool operator==(const Token &token) const {
            return this->type == token.type
                   && this->colno == token.colno
                   && this->lineno == token.lineno
                   && this->value == token.value;
        }

        std::string String() const {
            char tmp[100];
            sprintf(tmp, "<L:%d, C:%d, %s>\t\t%s", this->lineno, this->colno, TokenStringValue.at(this->type),
                    this->value.substr(0, 62).c_str());
            return tmp;
        }
    };

    using TokenUptr = std::unique_ptr<Token>;

}  // namespace lang::scanner

#endif //!ARGON_LANG_SCANNER_TOKEN_H_