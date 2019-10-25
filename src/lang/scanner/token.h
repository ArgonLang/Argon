// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_TOKEN_H_
#define ARGON_LANG_SCANNER_TOKEN_H_

#include <memory>
#include <string>

namespace lang::scanner {

	enum class TokenType { 
		END_OF_FILE, 
		END_OF_LINE,
		EXCLAMATION,
		NOT_EQUAL,
		STRING,
		BYTE_STRING,
		RAW_STRING,
		HASH,
		PERCENT,
		AMPERSAND,
		AND,
		// SINGLE QUOTE
		LEFT_ROUND,
		RIGHT_ROUND,
		ASTERISK,
		PLUS,
		COMMA,
		MINUS,
		DOT,
		ELLIPSIS,
		FRACTION_SLASH,
		NUMBER_BIN, 
		NUMBER_OCT, 
		NUMBER_HEX, 
		NUMBER, 
		DECIMAL,
		COLON,
		SEMICOLON,
		LESS,
		LESS_EQ,
		EQUAL,
		GREATER,
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

	struct Token {
		TokenType type = TokenType::END_OF_FILE;
		unsigned colno = 0;
		unsigned lineno = 0;
		std::string value;

		Token(Token&&) = default;

		Token() = default;

		Token(TokenType type, unsigned colno, unsigned lineno, const std::string& value) {
			this->type = type;
			this->colno = colno;
			this->lineno = lineno;
			this->value = value;
		}

		Token& operator=(Token&& token) noexcept {
			this->type = token.type;
			this->colno = token.colno;
			this->lineno = token.lineno;
			this->value = token.value;
			return *this;
		}

		bool operator==(const Token& token) const {
			return this->type == token.type
				&& this->colno == token.colno
				&& this->lineno == token.lineno
				&& this->value == token.value;
		}
	};

	using TokenUptr = std::unique_ptr<Token>;

}  // namespace lang::scanner

#endif //!ARGON_LANG_SCANNER_TOKEN_H_