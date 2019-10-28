// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_SCANNER_H_
#define ARGON_LANG_SCANNER_SCANNER_H_

#include <iostream>

#include "token.h"

namespace lang::scanner {
	constexpr bool IsSpace (int chr) { return chr == 0x09 || chr == 0x20; }
	constexpr bool IsAlpha(int chr) { return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || chr=='_'; }
	constexpr bool IsDigit(int chr) { return chr >= '0' && chr <= '9'; }
	constexpr bool IsOctDigit(int chr) { return chr >= '0' && chr <= '7'; }
	constexpr bool IsHexDigit(int chr) { return (chr >= '0' && chr <= '9') || (tolower(chr) >= 'a' && tolower(chr) <= 'f'); }
	constexpr unsigned char HexDigitToNumber(int chr) { return (IsDigit(chr)) ? ((char)chr) - '0' : 10 + (tolower(chr) - 'a'); }

	class Scanner {
	private:
		std::istream* source_;
		unsigned colno_ = 0;
		unsigned lineno_ = 0;

		bool ParseEscape(int stopChr, bool ignore_unicode_escape, std::string& dest, std::string& error);

		bool ParseUnicodeEscape(std::string& dest, std::string& error, bool extended); 

		bool ParseOctEscape(std::string& dest, std::string& error, int value);

		bool ParseHexEscape(std::string& dest, std::string& error);

		bool ParseHexToByte(unsigned char& byte);

		std::string ParseComment(bool inline_comment);

		Token Emit(TokenType type, const std::string& value);

		Token Emit(TokenType type, unsigned lineno, const std::string& value);

		Token ParseBinary();

		Token ParseOctal();

		Token ParseHex();

		Token ParseDecimal();

		Token ParseNumber();

		Token ParseString(TokenType type, bool ignore_unicode_escape);

		Token ParseRawString();

		Token ParseWord();

		int GetCh();

	public:
		explicit Scanner(std::istream* source) : source_(source) {};

		lang::scanner::Token NextToken();
	};
}  // namespace lang::scanner

#endif // !ARGON_LANG_SCANNER_SCANNER_H_