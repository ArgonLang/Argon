// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_TOKEN_H_
#define ARGON_LANG_SCANNER_TOKEN_H_

#include <memory>
#include <string>

namespace lang::scanner {

	enum class TokenType { END_OF_FILE, END_OF_LINE};

	struct Token {
		TokenType type;
		unsigned colno;
		unsigned lineno;
		std::string value;

		Token(Token&&) = default;

		Token(TokenType type, unsigned colno, unsigned lineno, const std::string& value) {
			this->type = type;
			this->colno = colno;
			this->lineno = lineno;
			this->value = value;
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