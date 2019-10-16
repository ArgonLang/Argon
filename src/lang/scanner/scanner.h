// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_SCANNER_H_
#define ARGON_LANG_SCANNER_SCANNER_H_

#include <iostream>

#include "token.h"

namespace lang::scanner {
	constexpr bool IsSpace (int chr) { return chr == 0x09 || chr == 0x20; }
	constexpr bool IsDigit(int chr) { return chr >= '0' && chr <= '9'; }
	constexpr bool IsHexDigit(int chr) { return (chr >= '0' && chr <= '9') || (tolower(chr) >= 'a' && tolower(chr) <= 'f'); }

	class Scanner {
	private:
		std::istream& source_;
		unsigned colno_ = 0;
		unsigned lineno_ = 0;

		Token Emit(TokenType type, const std::string& value);

		Token Emit(TokenType type, unsigned lineno, const std::string& value);

		Token ParseBinary();

		Token ParseOctal();

		Token ParseHex();

		Token ParseDecimal();

		Token ParseNumber();

		int Skip(unsigned char byte);

		int GetCh();

	public:
		explicit Scanner(std::istream& source) : source_(source) {};

		lang::scanner::Token NextToken();
	};
}  // namespace lang::scanner

#endif // !ARGON_LANG_SCANNER_SCANNER_H_