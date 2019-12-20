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
        TK_NULL,
        END_OF_FILE,
        END_OF_LINE,
        EXCLAMATION,
        EXCLAMATION_DOT,
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
        SLASH,
        SLASH_SLASH,
        SLASH_EQ,

        NUMBER_BEGIN,
        NUMBER,
        NUMBER_BIN,
        NUMBER_OCT,
        NUMBER_HEX,
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
        EQUAL,
        EQUAL_EQUAL,
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
        ATOMIC,
        BREAK,
        CASE,
        CONTINUE,
        DEFAULT,
        DEFER,
        ELIF,
        ELSE,
        FALLTHROUGH,
        FALSE,
        FOR,
        FUNC,
        GOTO,
        IF,
        IMPL,
        IMPORT,
        LET,
        LOOP,
        NIL,
        PUB,
        RETURN,
        SPAWN,
        STRUCT,
        SWITCH,
        TRAIT,
        TRUE,
        USING,
        VAR,
        WEAK,
        KEYWORD_END,
        ERROR
    };

    static const std::map<TokenType, std::string> TokenStringValue = {
            {TokenType::END_OF_FILE,     "EOF"},
            {TokenType::END_OF_LINE,     "EOL"},
            {TokenType::EXCLAMATION,     "EXCLAMATION"},
            {TokenType::EXCLAMATION_DOT, "EXCLAMATION_DOT"},
            {TokenType::NOT_EQUAL,       "NOT_EQUAL"},
            {TokenType::STRING,          "STRING"},
            {TokenType::BYTE_STRING,     "BYTE_STRING"},
            {TokenType::RAW_STRING,      "RAW_STRING"},
            {TokenType::INLINE_COMMENT,  "INLINE_COMMENT"},
            {TokenType::COMMENT,         "COMMENT"},
            {TokenType::PERCENT,         "PERCENT"},
            {TokenType::AMPERSAND,       "AMPERSAND"},
            {TokenType::AND,             "AND"},
            // SINGLE QUOTE
            {TokenType::LEFT_ROUND,      "LEFT_ROUND"},
            {TokenType::RIGHT_ROUND,     "RIGHT_ROUND"},
            {TokenType::ASTERISK,        "ASTERISK"},
            {TokenType::ASTERISK_EQ,     "ASTERISK_EQ"},
            {TokenType::PLUS,            "PLUS"},
            {TokenType::PLUS_PLUS,       "PLUS_PLUS"},
            {TokenType::PLUS_EQ,         "PLUS_EQ"},
            {TokenType::COMMA,           "COMMA"},
            {TokenType::MINUS,           "MINUS"},
            {TokenType::MINUS_MINUS,     "MINUS_MINUS"},
            {TokenType::MINUS_EQ,        "MINUS_EQ"},
            {TokenType::DOT,             "DOT"},
            {TokenType::ELLIPSIS,        "ELLIPSIS"},
            {TokenType::SLASH,           "SLASH"},
            {TokenType::SLASH_SLASH,     "SLASH_SLASH"},
            {TokenType::SLASH_EQ,        "SLASH_EQ"},
            {TokenType::NUMBER_BIN,      "NUMBER_BIN"},
            {TokenType::NUMBER_OCT,      "NUMBER_OCT"},
            {TokenType::NUMBER_HEX,      "NUMBER_HEX"},
            {TokenType::NUMBER,          "NUMBER"},
            {TokenType::DECIMAL,         "DECIMAL"},
            {TokenType::COLON,           "COLON"},
            {TokenType::SCOPE,           "SCOPE"},
            {TokenType::SEMICOLON,       "SEMICOLON"},
            {TokenType::LESS,            "LESS"},
            {TokenType::SHL,             "SHL"},
            {TokenType::LESS_EQ,         "LESS_EQ"},
            {TokenType::EQUAL,           "EQUAL"},
            {TokenType::EQUAL_EQUAL,     "EQUAL_EQUAL"},
            {TokenType::GREATER,         "GREATER"},
            {TokenType::SHR,             "SHR"},
            {TokenType::GREATER_EQ,      "GREATER_EQ"},
            {TokenType::QUESTION,        "QUESTION"},
            {TokenType::QUESTION_DOT,    "QUESTION_DOT"},
            {TokenType::ELVIS,           "ELVIS"},
            {TokenType::IDENTIFIER,      "IDENTIFIER"},
            {TokenType::LEFT_SQUARE,     "LEFT_SQUARE"},
            {TokenType::RIGHT_SQUARE,    "RIGHT_SQUARE"},
            {TokenType::CARET,           "CARET"},
            // `
            {TokenType::LEFT_BRACES,     "LEFT_BRACES"},
            {TokenType::PIPE,            "PIPE"},
            {TokenType::OR,              "OR"},
            {TokenType::RIGHT_BRACES,    "RIGHT_BRACES"},
            {TokenType::TILDE,           "TILDE"},
            {TokenType::FALSE,           "FALSE"},
            {TokenType::TRUE,            "TRUE"},
            {TokenType::ERROR,           "ERROR"}
    };

    static const std::map<std::string, TokenType> Keywords = {
            {"as",          TokenType::AS},
            {"atomic",      TokenType::ATOMIC},
            {"break",       TokenType::BREAK},
            {"case",        TokenType::CASE},
            {"continue",    TokenType::CONTINUE},
            {"default",     TokenType::DEFAULT},
            {"defer",       TokenType::DEFER},
            {"elif",        TokenType::ELIF},
            {"else",        TokenType::ELSE},
            {"fallthrough", TokenType::FALLTHROUGH},
            {"false",       TokenType::FALSE},
            {"for",         TokenType::FOR},
            {"func",        TokenType::FUNC},
            {"goto",        TokenType::GOTO},
            {"if",          TokenType::IF},
            {"impl",        TokenType::IMPL},
            {"import",      TokenType::IMPORT},
            {"let",         TokenType::LET},
            {"loop",        TokenType::LOOP},
            {"nil",         TokenType::NIL},
            {"pub",         TokenType::PUB},
            {"return",      TokenType::RETURN},
            {"spawn",       TokenType::SPAWN},
            {"struct",      TokenType::STRUCT},
            {"switch",      TokenType::SWITCH},
            {"trait",       TokenType::TRAIT},
            {"true",        TokenType::TRUE},
            {"using",       TokenType::USING},
            {"var",         TokenType::VAR},
            {"weak",        TokenType::WEAK}
    };

    struct Token {
        TokenType type = TokenType::TK_NULL;
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

        [[nodiscard]] std::string String() const {
            char tmp[142];
            std::string tkType;

            if (this->type > TokenType::KEYWORD_BEGIN && this->type < TokenType::KEYWORD_END)
                tkType = "KEYWORD";
            else
                tkType = TokenStringValue.at(this->type);

            sprintf(tmp, "<L:%d, C:%d, %s>\t\t%s", this->lineno, this->colno, tkType.c_str(),
                    this->value.substr(0, 62).c_str());
            return tmp;
        }
    };

    using TokenUptr = std::unique_ptr<Token>;

}  // namespace lang::scanner

#endif //!ARGON_LANG_SCANNER_TOKEN_H_