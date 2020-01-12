// This source file is part of the Argon project.
//
// Licensed under the Apache License v2.0

#ifndef ARGON_LANG_SCANNER_SCANNER_H_
#define ARGON_LANG_SCANNER_SCANNER_H_

#include <iostream>

#include "token.h"

namespace lang::scanner {
    constexpr bool IsSpace(int chr) { return chr == 0x09 || chr == 0x20; }

    constexpr bool IsAlpha(int chr) { return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || chr == '_'; }

    constexpr bool IsDigit(int chr) { return chr >= '0' && chr <= '9'; }

    constexpr bool IsOctDigit(int chr) { return chr >= '0' && chr <= '7'; }

    constexpr bool IsHexDigit(int chr) {
        return (chr >= '0' && chr <= '9') || (tolower(chr) >= 'a' && tolower(chr) <= 'f');
    }

    constexpr unsigned char HexDigitToNumber(int chr) {
        return (IsDigit(chr)) ? ((char) chr) - '0' : 10 + (tolower(chr) - 'a');
    }

    class Scanner {
    private:
        Token peeked_token_;
        std::istream *source_;
        bool peeked_ = false;
        int colno_ = 0;
        int lineno_ = 0;
        Pos pos_ = 1;

        bool ParseEscape(int stopChr, bool ignore_unicode_escape, std::string &dest, std::string &error);

        bool ParseUnicodeEscape(std::string &dest, std::string &error, bool extended);

        bool ParseOctEscape(std::string &dest, std::string &error, int value);

        bool ParseHexEscape(std::string &dest, std::string &error);

        bool ParseHexToByte(unsigned char &byte);

        std::string ParseComment(bool inline_comment);

        Token ParseBinary(Pos start);

        Token ParseOctal(Pos start);

        Token ParseHex(Pos start);

        Token ParseDecimal(Pos start);

        Token ParseNumber();

        Token ParseString(int colno, bool byte_string);

        Token ParseRawString(int colno, int lineno);

        Token ParseWord();

        Token NextToken();

        int GetCh();

    public:
        explicit Scanner(std::istream *source) : source_(source) {};

        Token Peek();

        Token Next();
    };
}  // namespace lang::scanner

#endif // !ARGON_LANG_SCANNER_SCANNER_H_